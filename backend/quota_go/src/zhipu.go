package main

import (
	"encoding/json"
	"fmt"
	"net/http"
	"strings"
	"time"
)

const zhipuUsageURL = "https://bigmodel.cn/api/monitor/usage/quota/limit"

func fetchZhipu() (map[string]interface{}, error) {
	apiKey := LoadConfig().Zhipu.APIKey
	if apiKey == "" || apiKey == "在此填入您的智谱 API Key" {
		return nil, fmt.Errorf("请先在 .config.json 中配置 zhipu.api_key")
	}

	req, err := http.NewRequest("GET", zhipuUsageURL, nil)
	if err != nil {
		return nil, fmt.Errorf("创建请求失败: %s", err)
	}
	req.Header.Set("Authorization", "Bearer "+apiKey)
	resp, err := http.DefaultClient.Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()

	if resp.StatusCode != 200 {
		return nil, fmt.Errorf("HTTP %d", resp.StatusCode)
	}

	var data map[string]interface{}
	if err := json.NewDecoder(resp.Body).Decode(&data); err != nil {
		return nil, fmt.Errorf("解析响应失败: %s", err)
	}
	return data, nil
}

func showZhipu(data map[string]interface{}) string {
	if code, ok := data["code"].(float64); !ok || int(code) != 200 {
		return fmt.Sprintf("  错误: %v", data)
	}
	lines := ""
	dd, _ := data["data"].(map[string]interface{})
	limits, _ := dd["limits"].([]interface{})
	for _, l := range limits {
		limit, _ := l.(map[string]interface{})
		ltype, _ := limit["type"].(string)
		pct := 0
		if f, ok := limit["percentage"].(float64); ok {
			pct = int(f)
		}
		remaining := 100 - pct
		var resetStr string
		if ms, ok := limit["nextResetTime"].(float64); ok && ms > 0 {
			resetStr = fmtReset(time.Unix(int64(ms/1000), 0))
		}
		switch ltype {
		case "TOKENS_LIMIT":
			unit := 0
			if f, ok := limit["unit"].(float64); ok {
				unit = int(f)
			}
			if unit == 3 {
				lines += fmt.Sprintf("  每5小时额度 剩余 %d%% 重置时间：%s\n", remaining, resetStr)
			} else if unit == 6 {
				lines += fmt.Sprintf("  每周额度 剩余 %d%% 重置时间：%s\n", remaining, resetStr)
			}
		case "TIME_LIMIT":
			lines += fmt.Sprintf("  MCP每月额度 剩余 %d%% 重置时间：%s\n", remaining, resetStr)
		}
	}
	return strings.TrimRight(lines, "\n")
}

func extractZhipu(raw map[string]interface{}) map[string]interface{} {
	result := map[string]interface{}{"p": "zhipu"}
	dd, _ := raw["data"].(map[string]interface{})
	limits, _ := dd["limits"].([]interface{})
	for _, l := range limits {
		limit, _ := l.(map[string]interface{})
		ltype, _ := limit["type"].(string)
		pct := 0
		if f, ok := limit["percentage"].(float64); ok {
			pct = int(f)
		}
		remaining := 100 - pct
		var resetStr string
		if remaining < 100 {
			if ms, ok := limit["nextResetTime"].(float64); ok && ms > 0 {
				resetStr = fmtReset(time.Unix(int64(ms/1000), 0))
			}
		}
		switch ltype {
		case "TOKENS_LIMIT":
			unit := 0
			if f, ok := limit["unit"].(float64); ok {
				unit = int(f)
			}
			if unit == 3 {
				result["h5"] = remaining
				result["h5r"] = resetStr
			} else if unit == 6 {
				result["w"] = remaining
				result["wr"] = resetStr
			}
		case "TIME_LIMIT":
			result["mcp"] = remaining
			result["mcpr"] = resetStr
		}
	}
	return result
}
