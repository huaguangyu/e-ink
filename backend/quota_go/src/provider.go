package main

import (
	"encoding/json"
	"fmt"
	"os"
)

type Provider struct {
	Name        string
	Key         string
	Fetch       func() (map[string]interface{}, error)
	Show        func(map[string]interface{}) string
	Extract     func(map[string]interface{}) map[string]interface{}
}

var providers = map[string]Provider{
	"gemini": {"Antigravity", "gemini", fetchGemini, showGemini, extractGemini},
	"codex":  {"Codex/ChatGPT", "codex", fetchCodex, showCodex, extractCodex},
	"zhipu":  {"智谱 GLM", "zhipu", fetchZhipu, showZhipu, extractZhipu},
}

func providerOrder() []string {
	return []string{"gemini", "codex", "zhipu"}
}

func doFetch(target string) {
	ensureCacheDir()
	tags := providerOrder()
	if target != "all" {
		tags = []string{target}
	}
	for _, tag := range tags {
		p, ok := providers[tag]
		if !ok {
			fmt.Printf("  未知平台: %s\n", tag)
			continue
		}
		if IsDisabled(tag) {
			fmt.Printf("  %s 已禁用，跳过\n", tag)
			continue
		}
		// Zhipu reads API key from .config.json, not an account file.
		if tag != "zhipu" {
			if _, err := os.Stat(AccountPath(tag)); os.IsNotExist(err) {
				fmt.Printf("  %s 账号文件不存在，跳过\n", tag)
				continue
			}
		}
		raw, err := p.Fetch()
		if err != nil {
			fmt.Printf("  %s 获取失败: %s\n", p.Name, err)
			continue
		}
		data, _ := json.MarshalIndent(raw, "", "  ")
		os.WriteFile(CachePath(tag), data, 0644)
		fmt.Printf("  %s 数据已更新\n", p.Name)
	}
}

func doShow(target string) {
	tags := providerOrder()
	if target != "all" {
		tags = []string{target}
	}
	for _, tag := range tags {
		p := providers[tag]
		fmt.Printf("\n%s\n", separator())
		fmt.Printf("  %s\n", p.Name)
		fmt.Printf("%s\n", separator())
		data, err := loadJSON(CachePath(tag))
		if err != nil {
			fmt.Printf("  无缓存数据，请先运行: quota_checker fetch %s\n", tag)
			continue
		}
		fmt.Println(p.Show(data))
	}
	fmt.Println()
}

func separator() string {
	return "=================================================="
}
