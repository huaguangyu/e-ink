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

const defaultConfigJSON = `{
  "port": 12001,
  "server_path": "/LimiT/quErY",
  "cache_ttl": 600,
  "disabled": [],
  "gemini": {
    "client_id": "1071006060591-tmhssin2h21lcre235vtolojh4g403ep.apps.googleusercontent.com",
    "client_secret": "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",
    "oauth_ua": "vscode/1.X.X (Antigravity/4.2.1)",
    "api_ua": "Antigravity/4.2.1 (Macintosh; Intel Mac OS X 10_15_7) Chrome/132.0.6834.160 Electron/39.2.3"
  },
  "_comment_gemini": "client_id 和 client_secret 是 Antigravity 项目的固定 OAuth 凭据，无需修改",
  "codex": {
    "client_id": "app_EMoamEEZ73f0CkXaXp7hrann",
    "ua": "codex_cli_rs/0.118.0 (Mac OS 26.3.1; arm64) iTerm.app/3.6.9"
  },
  "_comment_codex": "client_id 和 ua 是 Codex CLI 固定标识，无需修改",
  "zhipu": {
    "api_key": "在此填入您的智谱 API Key"
  },
  "notify": {
    "sendkey": ""
  },
  "accounts": {
    "gemini": "auths/gemini_account.json",
    "codex": "auths/codex_account.json"
  }
}`

func LoadConfig() *Config {
	cfgOnce.Do(func() {
		cfg = &Config{
			Port:       12001,
			ServerPath: "/LimiT/quErY",
			CacheTTL:   600,
			Disabled:   []string{},
			Gemini: GeminiConfig{
				ClientID:     "1071006060591-tmhssin2h21lcre235vtolojh4g403ep.apps.googleusercontent.com",
				ClientSecret: "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",
				OAuthUA:      "vscode/1.X.X (Antigravity/4.2.1)",
				APIUA:        "Antigravity/4.2.1 (Macintosh; Intel Mac OS X 10_15_7) Chrome/132.0.6834.160 Electron/39.2.3",
			},
			Codex: CodexConfig{
				ClientID: "app_EMoamEEZ73f0CkXaXp7hrann",
				UA:       "codex_cli_rs/0.118.0 (Mac OS 26.3.1; arm64) iTerm.app/3.6.9",
			},
			Zhipu: ZhipuConfig{
				APIKey: "在此填入您的智谱 API Key",
			},
			Accounts: defaultAccounts(),
		}
		path := filepath.Join(configDir, ".config.json")
		data, err := os.ReadFile(path)
		if err != nil {
			// Config file doesn't exist yet — write template so user can edit it.
			if werr := os.WriteFile(path, []byte(defaultConfigJSON), 0600); werr != nil {
				fmt.Printf("创建模板配置文件失败: %s\n", werr)
			} else {
				fmt.Printf("未找到配置文件，已创建模板: %s\n请编辑后重新启动\n", path)
			}
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
		// Gemini/Codex 固定凭据：用户无需手动填写，缺失时自动补全
		if cfg.Gemini.ClientID == "" {
			cfg.Gemini.ClientID = "1071006060591-tmhssin2h21lcre235vtolojh4g403ep.apps.googleusercontent.com"
		}
		if cfg.Gemini.ClientSecret == "" {
			cfg.Gemini.ClientSecret = "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"
		}
		if cfg.Gemini.OAuthUA == "" {
			cfg.Gemini.OAuthUA = "vscode/1.X.X (Antigravity/4.2.1)"
		}
		if cfg.Gemini.APIUA == "" {
			cfg.Gemini.APIUA = "Antigravity/4.2.1 (Macintosh; Intel Mac OS X 10_15_7) Chrome/132.0.6834.160 Electron/39.2.3"
		}
		if cfg.Codex.ClientID == "" {
			cfg.Codex.ClientID = "app_EMoamEEZ73f0CkXaXp7hrann"
		}
		if cfg.Codex.UA == "" {
			cfg.Codex.UA = "codex_cli_rs/0.118.0 (Mac OS 26.3.1; arm64) iTerm.app/3.6.9"
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
