package main

import (
	"encoding/json"
	"fmt"
	"net/http"
	"os"
	"path/filepath"
	"sync"
	"time"
)

var (
	baseDir   string
	configDir string
	cacheDir  string
	authsDir  string
	httpClient *http.Client
)

func init() {
	exe, _ := os.Executable()
	baseDir = filepath.Dir(exe)
	// 支持 go run 时用工作目录
	if _, err := os.Stat(filepath.Join(baseDir, ".config.json")); err != nil {
		wd, _ := os.Getwd()
		if _, err2 := os.Stat(filepath.Join(wd, ".config.json")); err2 == nil {
			baseDir = wd
		}
	}
	configDir = baseDir
	cacheDir = filepath.Join(baseDir, "cache")
	authsDir = filepath.Join(baseDir, "auths")
	httpClient = &http.Client{Timeout: 30 * time.Second}
}

type GeminiConfig struct {
	ClientID     string `json:"client_id"`
	ClientSecret string `json:"client_secret"`
	OAuthUA      string `json:"oauth_ua"`
	APIUA        string `json:"api_ua"`
}

type CodexConfig struct {
	ClientID string `json:"client_id"`
	UA       string `json:"ua"`
}

type ZhipuConfig struct {
	APIKey string `json:"api_key"`
}

type NotifyConfig struct {
	SendKey string `json:"sendkey"`
}

type Config struct {
	Port       int               `json:"port"`
	ServerPath string            `json:"server_path"`
	CacheTTL   int               `json:"cache_ttl"`
	Disabled   []string          `json:"disabled"`
	Gemini     GeminiConfig      `json:"gemini"`
	Codex      CodexConfig       `json:"codex"`
	Zhipu      ZhipuConfig       `json:"zhipu"`
	Notify     NotifyConfig      `json:"notify"`
	Accounts   map[string]string `json:"accounts"`
}

var (
	cfg     *Config
	cfgOnce sync.Once
)

func defaultAccounts() map[string]string {
	return map[string]string{
		"gemini": "auths/gemini_account.json",
		"codex":  "auths/codex_account.json",
	}
}

func LoadConfig() *Config {
	cfgOnce.Do(func() {
		cfg = &Config{
			Port:       12001,
			ServerPath: "/LimiT/quErY",
			CacheTTL:   600,
			Disabled:   []string{},
			Zhipu: ZhipuConfig{
				APIKey: "在此填入您的智谱 API Key",
			},
			Accounts: defaultAccounts(),
		}
		path := filepath.Join(configDir, ".config.json")
		data, err := os.ReadFile(path)
		if err != nil {
			// Config file doesn't exist yet — write template so user can edit it.
			saveJSON(path, cfg)
			fmt.Printf("未找到配置文件，已创建模板: %s\n请编辑后重新启动\n", path)
			return
		}
		json.Unmarshal(data, cfg)
		if cfg.Port == 0 {
			cfg.Port = 12001
		}
		if cfg.ServerPath == "" {
			cfg.ServerPath = "/LimiT/quErY"
		}
		if cfg.CacheTTL == 0 {
			cfg.CacheTTL = 600
		}
		if cfg.Accounts == nil {
			cfg.Accounts = defaultAccounts()
		} else {
			def := defaultAccounts()
			for k, v := range def {
				if _, ok := cfg.Accounts[k]; !ok {
					cfg.Accounts[k] = v
				}
			}
		}
	})
	return cfg
}

func IsDisabled(provider string) bool {
	for _, d := range LoadConfig().Disabled {
		if d == provider {
			return true
		}
	}
	return false
}

func AccountPath(provider string) string {
	rel := LoadConfig().Accounts[provider]
	return filepath.Join(baseDir, rel)
}

func CachePath(provider string) string {
	return filepath.Join(cacheDir, provider+".json")
}

func ensureCacheDir() {
	os.MkdirAll(cacheDir, 0755)
}

// ensureRuntimeDirs creates auths/ and cache/ if they don't exist.
func ensureRuntimeDirs() {
	os.MkdirAll(authsDir, 0755)
	os.MkdirAll(cacheDir, 0755)
}

// ensureAccountTemplates creates placeholder account files for OAuth-based
// providers (gemini, codex) so the user knows the expected format.
func ensureAccountTemplates() {
	placeholders := map[string]map[string]interface{}{
		"gemini_account.json": {"refresh_token": "在此填入 Google OAuth refresh_token"},
		"codex_account.json":  {"refresh_token": "在此填入 OpenAI OAuth refresh_token"},
	}
	for name, content := range placeholders {
		path := filepath.Join(authsDir, name)
		if _, err := os.Stat(path); os.IsNotExist(err) {
			saveJSON(path, content)
			fmt.Printf("已创建模板文件: %s\n", path)
		}
	}
}

func loadJSON(path string) (map[string]interface{}, error) {
	data, err := os.ReadFile(path)
	if err != nil {
		return nil, err
	}
	var result map[string]interface{}
	err = json.Unmarshal(data, &result)
	return result, err
}

func saveJSON(path string, v interface{}) error {
	data, err := json.MarshalIndent(v, "", "  ")
	if err != nil {
		return fmt.Errorf("序列化失败: %s", err)
	}
	return os.WriteFile(path, data, 0600)
}
