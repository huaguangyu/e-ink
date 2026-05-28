package main

import (
	"bytes"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"net/http"
	"net/url"
	"strings"
	"time"
)

const agTokenURL = "https://oauth2.googleapis.com/token"

type httpStatusError struct {
	Code int
}

func (e *httpStatusError) Error() string { return fmt.Sprintf("HTTP %d", e.Code) }

var agEndpoints = []string{
	"https://daily-cloudcode-pa.sandbox.googleapis.com/v1internal:fetchAvailableModels",
	"https://daily-cloudcode-pa.googleapis.com/v1internal:fetchAvailableModels",
	"https://cloudcode-pa.googleapis.com/v1internal:fetchAvailableModels",
}

func agEnsureToken(cfg map[string]interface{}) (map[string]interface{}, error) {
	needRefresh := true
	if exp, ok := cfg["expired"].(string); ok && exp != "" {
		t, err := time.Parse(time.RFC3339Nano, exp)
		if err == nil && time.Now().Before(t) {
			needRefresh = false
		}
	}

	if needRefresh {
		gc := LoadConfig().Gemini
		form := url.Values{
			"client_id":     {gc.ClientID},
			"client_secret": {gc.ClientSecret},
			"refresh_token": {fmt.Sprint(cfg["refresh_token"])},
			"grant_type":    {"refresh_token"},
		}
		req, err := http.NewRequest("POST", agTokenURL, strings.NewReader(form.Encode()))
		if err != nil {
			return cfg, fmt.Errorf("创建请求失败: %s", err)
		}
		req.Header.Set("Content-Type", "application/x-www-form-urlencoded")
		req.Header.Set("User-Agent", gc.OAuthUA)
		resp, err := httpClient.Do(req)
		if err != nil {
			return cfg, fmt.Errorf("刷新失败: %s", err)
		}
		defer resp.Body.Close()
		if resp.StatusCode != 200 {
			body, _ := io.ReadAll(resp.Body)
			if strings.Contains(string(body), "invalid_grant") {
				return cfg, &PermanentError{Msg: "refresh_token 已失效"}
			}
			return cfg, fmt.Errorf("刷新失败: HTTP %d", resp.StatusCode)
		}
		var td map[string]interface{}
		if err := json.NewDecoder(resp.Body).Decode(&td); err != nil {
			return cfg, fmt.Errorf("解析响应失败: %s", err)
		}
		cfg["access_token"] = td["access_token"]
		expiresIn := 3600.0
		if v, ok := td["expires_in"].(float64); ok {
			expiresIn = v
		}
		cfg["expired"] = time.Now().Add(time.Duration(expiresIn) * time.Second).Format(time.RFC3339Nano)
		if rt, ok := td["refresh_token"].(string); ok {
			cfg["refresh_token"] = rt
		}
	}
	return cfg, nil
}

func agFetchEp(ep, token, ua string) (map[string]interface{}, error) {
	req, err := http.NewRequest("POST", ep, bytes.NewReader([]byte("{}")))
	if err != nil {
		return nil, err
	}
	req.Header.Set("Authorization", "Bearer "+token)
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("User-Agent", ua)
	resp, err := httpClient.Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()
	if resp.StatusCode != 200 {
		return nil, &httpStatusError{Code: resp.StatusCode}
	}
	var raw map[string]interface{}
	if err := json.NewDecoder(resp.Body).Decode(&raw); err != nil {
		return nil, fmt.Errorf("解析响应失败: %s", err)
	}
	return raw, nil
}

func fetchGemini() (map[string]interface{}, error) {
	path := AccountPath("gemini")
	cfg, err := loadJSON(path)
	if err != nil {
		return nil, fmt.Errorf("读取凭据失败: %s", err)
	}
	cfg, err = agEnsureToken(cfg)
	if err != nil {
		return nil, err
	}

	gc := LoadConfig().Gemini
	token := fmt.Sprint(cfg["access_token"])
	var raw map[string]interface{}

	for idx, ep := range agEndpoints {
		r, err := agFetchEp(ep, token, gc.APIUA)
		if err != nil {
			var hse *httpStatusError
			if errors.As(err, &hse) {
				if (hse.Code == 429 || hse.Code >= 500) && idx+1 < len(agEndpoints) {
					time.Sleep(time.Second)
					continue
				}
				if hse.Code == 401 {
					break
				}
			}
			continue
		}
		raw = r
		break
	}

	if raw == nil {
		cfg["expired"] = time.Now().Format(time.RFC3339Nano)
		cfg, err = agEnsureToken(cfg)
		if err != nil {
			return nil, err
		}
		saveJSON(path, cfg)
		token = fmt.Sprint(cfg["access_token"])
		for _, ep := range agEndpoints {
			r, err := agFetchEp(ep, token, gc.APIUA)
			if err == nil {
				raw = r
				break
			}
		}
	}

	if raw == nil {
		return nil, fmt.Errorf("所有端点均失败")
	}
	saveJSON(path, cfg)
	return raw, nil
}

func showGemini(data map[string]interface{}) string {
	models, _ := data["models"].(map[string]interface{})
	lines := ""
	type item struct {
		id    string
		label string
	}
	for _, it := range []item{{"gemini-pro-agent", "Gemini"}, {"claude-opus-4-6-thinking", "Claude"}} {
		info, ok := models[it.id].(map[string]interface{})
		if !ok {
			lines += fmt.Sprintf("  %s  (未找到)\n", it.label)
			continue
		}
		qi, _ := info["quotaInfo"].(map[string]interface{})
		pct := 0
		if f, ok := qi["remainingFraction"].(float64); ok {
			pct = int(f * 100)
		}
		resetStr := "N/A"
		if resetUTC, ok := qi["resetTime"].(string); ok {
			t, err := time.Parse(time.RFC3339Nano, resetUTC)
			if err == nil {
				resetStr = fmtReset(t)
			}
		}
		lines += fmt.Sprintf("  %s 剩余 %d%% 重置时间：%s\n", it.label, pct, resetStr)
	}
	return strings.TrimRight(lines, "\n")
}

func extractGemini(raw map[string]interface{}) map[string]interface{} {
	result := map[string]interface{}{"p": "gemini"}
	models, _ := raw["models"].(map[string]interface{})
	for _, it := range []struct {
		id  string
		tag string
	}{
		{"gemini-pro-agent", "gm"},
		{"claude-opus-4-6-thinking", "cl"},
	} {
		info, ok := models[it.id].(map[string]interface{})
		if !ok {
			continue
		}
		qi, _ := info["quotaInfo"].(map[string]interface{})
		pct := 0
		if f, ok := qi["remainingFraction"].(float64); ok {
			pct = int(f * 100)
		}
		result[it.tag] = pct
		if pct >= 100 {
			result[it.tag+"r"] = ""
		} else if resetUTC, ok := qi["resetTime"].(string); ok {
			t, err := time.Parse(time.RFC3339Nano, resetUTC)
			if err == nil {
				result[it.tag+"r"] = fmtReset(t)
			} else {
				result[it.tag+"r"] = ""
			}
		} else {
			result[it.tag+"r"] = ""
		}
	}
	return result
}
