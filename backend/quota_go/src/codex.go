package main

import (
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"net/url"
	"strings"
	"time"
)

const codexTokenURL = "https://auth.openai.com/oauth/token"
const codexUsageURL = "https://chatgpt.com/backend-api/wham/usage"

func codexEnsureToken(cfg map[string]interface{}) (map[string]interface{}, error) {
	needRefresh := true
	if exp, ok := cfg["expired"].(string); ok && exp != "" {
		t, err := time.Parse(time.RFC3339Nano, exp)
		if err == nil && time.Now().Before(t) {
			needRefresh = false
		}
	}

	if needRefresh {
		cc := LoadConfig().Codex
		form := url.Values{
			"client_id":     {cc.ClientID},
			"grant_type":    {"refresh_token"},
			"refresh_token": {fmt.Sprint(cfg["refresh_token"])},
		}
		req, err := http.NewRequest("POST", codexTokenURL, strings.NewReader(form.Encode()))
		if err != nil {
			return cfg, fmt.Errorf("创建请求失败: %s", err)
		}
		req.Header.Set("Content-Type", "application/x-www-form-urlencoded")
		req.Header.Set("User-Agent", cc.UA)
		resp, err := httpClient.Do(req)
		if err != nil {
			return cfg, fmt.Errorf("刷新失败: %s", err)
		}
		defer resp.Body.Close()
		if resp.StatusCode != 200 {
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
		if v, ok := td["id_token"].(string); ok {
			cfg["id_token"] = v
		}
		if v, ok := td["refresh_token"].(string); ok {
			cfg["refresh_token"] = v
		}
	}
	return cfg, nil
}

func fetchCodex() (map[string]interface{}, error) {
	path := AccountPath("codex")
	cfg, err := loadJSON(path)
	if err != nil {
		return nil, fmt.Errorf("读取凭据失败: %s", err)
	}
	cfg, err = codexEnsureToken(cfg)
	if err != nil {
		return nil, err
	}

	cc := LoadConfig().Codex
	token := fmt.Sprint(cfg["access_token"])

	doGet := func() (*http.Response, error) {
		req, err := http.NewRequest("GET", codexUsageURL, nil)
		if err != nil {
			return nil, fmt.Errorf("创建请求失败: %s", err)
		}
		req.Header.Set("Authorization", "Bearer "+token)
		req.Header.Set("User-Agent", cc.UA)
		return httpClient.Do(req)
	}

	resp, err := doGet()
	if err != nil {
		return nil, err
	}

	if resp.StatusCode == 401 {
		resp.Body.Close()
		cfg["expired"] = time.Now().Format(time.RFC3339Nano)
		cfg, err = codexEnsureToken(cfg)
		if err != nil {
			return nil, err
		}
		saveJSON(path, cfg)
		token = fmt.Sprint(cfg["access_token"])
		resp, err = doGet()
		if err != nil {
			return nil, err
		}
	}
	defer resp.Body.Close()

	if resp.StatusCode != 200 {
		body, _ := io.ReadAll(resp.Body)
		return nil, fmt.Errorf("HTTP %d: %s", resp.StatusCode, string(body))
	}

	var data map[string]interface{}
	if err := json.NewDecoder(resp.Body).Decode(&data); err != nil {
		return nil, fmt.Errorf("解析响应失败: %s", err)
	}

	if email, ok := data["email"].(string); ok {
		cfg["email"] = email
	}
	if pt, ok := data["plan_type"].(string); ok {
		cfg["plan_type"] = pt
	}
	saveJSON(path, cfg)
	return data, nil
}

func showCodex(data map[string]interface{}) string {
	rl, _ := data["rate_limit"].(map[string]interface{})
	lines := ""
	for _, it := range []struct {
		label string
		key   string
	}{
		{"5小时限额", "primary_window"},
		{"每周限额", "secondary_window"},
	} {
		w, ok := rl[it.key].(map[string]interface{})
		if !ok {
			continue
		}
		usedPct := 0
		if f, ok := w["used_percent"].(float64); ok {
			usedPct = int(f)
		}
		remaining := 100 - usedPct
		var resetAt float64
		if v, ok := w["reset_at"].(float64); ok {
			resetAt = v
		}
		resetStr := fmtReset(time.Unix(int64(resetAt), 0))
		lines += fmt.Sprintf("  %s 剩余 %d%% 重置时间：%s\n", it.label, remaining, resetStr)
	}
	return strings.TrimRight(lines, "\n")
}

func extractCodex(raw map[string]interface{}) map[string]interface{} {
	result := map[string]interface{}{"p": "codex"}
	rl, _ := raw["rate_limit"].(map[string]interface{})
	for _, it := range []struct {
		key string
		tag string
	}{
		{"primary_window", "h5"},
		{"secondary_window", "w"},
	} {
		w, ok := rl[it.key].(map[string]interface{})
		if !ok {
			continue
		}
		usedPct := 0
		if f, ok := w["used_percent"].(float64); ok {
			usedPct = int(f)
		}
		remaining := 100 - usedPct
		result[it.tag] = remaining
		if remaining >= 100 || (it.tag == "h5" && remaining == 99) {
			result[it.tag+"r"] = ""
		} else {
			var resetAt float64
			if v, ok := w["reset_at"].(float64); ok {
				resetAt = v
			}
			result[it.tag+"r"] = fmtReset(time.Unix(int64(resetAt), 0))
		}
	}
	return result
}
