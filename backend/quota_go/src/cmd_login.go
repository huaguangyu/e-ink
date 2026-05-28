package main

import (
	"bufio"
	"context"
	"crypto/rand"
	"crypto/sha256"
	"encoding/base64"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"net/url"
	"os"
	"path/filepath"
	"strings"
	"time"
)

// ── Codex (OpenAI) OAuth constants ──────────────────────────────────────────

const (
	codexAuthURL  = "https://auth.openai.com/oauth/authorize"
	codexClientID = "app_EMoamEEZ73f0CkXaXp7hrann"
	codexRedirect = "http://localhost:1455/auth/callback"
)

// ── Gemini (Antigravity) OAuth constants ────────────────────────────────────

const (
	geminiOAuthAuthURL  = "https://accounts.google.com/o/oauth2/v2/auth"

	geminiOAuthUserInfo = "https://www.googleapis.com/oauth2/v2/userinfo?alt=json"
	geminiOAuthClientID = "1071006060591-tmhssin2h21lcre235vtolojh4g403ep.apps.googleusercontent.com"
	geminiOAuthClientS  = "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"
	geminiOAuthRedirect = "http://localhost:51121/oauth-callback"
)

var geminiOAuthScopes = []string{
	"openid",
	"https://www.googleapis.com/auth/cloud-platform",
	"https://www.googleapis.com/auth/userinfo.email",
	"https://www.googleapis.com/auth/userinfo.profile",
	"https://www.googleapis.com/auth/cclog",
	"https://www.googleapis.com/auth/experimentsandconfigs",
}

// ── Top-level router ────────────────────────────────────────────────────────

func runLogin(args []string) {
	if len(args) == 0 {
		fmt.Println("用法: quota_checker login <codex|gemini>")
		os.Exit(1)
	}
	switch args[0] {
	case "codex":
		loginCodex()
	case "gemini":
		loginGemini()
	default:
		fmt.Printf("不支持的登录类型: %s\n支持: codex, gemini\n", args[0])
		os.Exit(1)
	}
}

// ── Shared helpers ──────────────────────────────────────────────────────────

func generateRandomState() (string, error) {
	b := make([]byte, 32)
	if _, err := rand.Read(b); err != nil {
		return "", err
	}
	return fmt.Sprintf("%x", b), nil
}

func promptCallbackURL() string {
	fmt.Println()
	fmt.Println("登录完成后，请将浏览器地址栏的完整 URL 粘贴到此处:")
	fmt.Print("> ")
	scanner := bufio.NewScanner(os.Stdin)
	scanner.Buffer(make([]byte, 4096), 4096)
	if !scanner.Scan() {
		fmt.Println("读取输入失败")
		os.Exit(1)
	}
	return strings.TrimSpace(scanner.Text())
}

func parseCallbackURL(raw string) (code, state string, err error) {
	raw = strings.TrimSpace(raw)
	if !strings.HasPrefix(raw, "http") {
		raw = "http://dummy/callback?" + raw
	}
	u, parseErr := url.Parse(raw)
	if parseErr != nil {
		return "", "", fmt.Errorf("无法解析回调 URL: %w", parseErr)
	}
	q := u.Query()
	code = strings.TrimSpace(q.Get("code"))
	state = strings.TrimSpace(q.Get("state"))
	if code == "" {
		return "", "", fmt.Errorf("回调 URL 中未找到 code 参数")
	}
	return code, state, nil
}

func writeAccountFile(filename string, data map[string]interface{}) error {
	os.MkdirAll(authsDir, 0700)
	path := filepath.Join(authsDir, filename)
	return saveJSON(path, data)
}

func doOAuthPOST(targetURL string, form url.Values) ([]byte, int, error) {
	req, err := http.NewRequestWithContext(context.Background(), "POST", targetURL, strings.NewReader(form.Encode()))
	if err != nil {
		return nil, 0, err
	}
	req.Header.Set("Content-Type", "application/x-www-form-urlencoded")
	req.Header.Set("Accept", "application/json")
	resp, err := httpClient.Do(req)
	if err != nil {
		return nil, 0, err
	}
	defer resp.Body.Close()
	body, _ := io.ReadAll(io.LimitReader(resp.Body, 1<<20))
	return body, resp.StatusCode, nil
}

// ── PKCE helpers (for Codex) ────────────────────────────────────────────────

func generatePKCECodes() (verifier, challenge string, err error) {
	b := make([]byte, 96)
	if _, err = rand.Read(b); err != nil {
		return "", "", err
	}
	verifier = base64.URLEncoding.WithPadding(base64.NoPadding).EncodeToString(b)
	h := sha256.Sum256([]byte(verifier))
	challenge = base64.URLEncoding.WithPadding(base64.NoPadding).EncodeToString(h[:])
	return verifier, challenge, nil
}

// ── JWT payload parser ──────────────────────────────────────────────────────

func parseJWTPayload(token string) (map[string]interface{}, error) {
	parts := strings.Split(token, ".")
	if len(parts) != 3 {
		return nil, fmt.Errorf("invalid JWT format")
	}
	payload := parts[1]
	switch len(payload) % 4 {
	case 2:
		payload += "=="
	case 3:
		payload += "="
	}
	data, err := base64.URLEncoding.DecodeString(payload)
	if err != nil {
		return nil, err
	}
	var m map[string]interface{}
	if err := json.Unmarshal(data, &m); err != nil {
		return nil, err
	}
	return m, nil
}

// ── Codex login ─────────────────────────────────────────────────────────────

func loginCodex() {
	fmt.Println("初始化 Codex (OpenAI) 登录...")

	verifier, challenge, err := generatePKCECodes()
	if err != nil {
		fmt.Printf("生成 PKCE 失败: %s\n", err)
		os.Exit(1)
	}

	state, err := generateRandomState()
	if err != nil {
		fmt.Printf("生成 state 失败: %s\n", err)
		os.Exit(1)
	}

	params := url.Values{
		"client_id":                  {codexClientID},
		"response_type":              {"code"},
		"redirect_uri":               {codexRedirect},
		"scope":                      {"openid email profile offline_access"},
		"state":                      {state},
		"code_challenge":             {challenge},
		"code_challenge_method":      {"S256"},
		"prompt":                     {"login"},
		"id_token_add_organizations": {"true"},
		"codex_cli_simplified_flow":  {"true"},
	}
	authURL := codexAuthURL + "?" + params.Encode()

	fmt.Println()
	fmt.Println("请复制以下链接到浏览器打开并登录:")
	fmt.Println()
	fmt.Println("  " + authURL)

	rawCallback := promptCallbackURL()
	code, cbState, err := parseCallbackURL(rawCallback)
	if err != nil {
		fmt.Printf("解析回调 URL 失败: %s\n", err)
		os.Exit(1)
	}
	if cbState != state {
		fmt.Printf("state 不匹配\n")
		os.Exit(1)
	}

	fmt.Println("正在交换 token...")
	form := url.Values{
		"grant_type":    {"authorization_code"},
		"client_id":     {codexClientID},
		"code":          {code},
		"redirect_uri":  {codexRedirect},
		"code_verifier": {verifier},
	}
	body, status, err := doOAuthPOST(codexTokenURL, form)
	if err != nil {
		fmt.Printf("Token 交换请求失败: %s\n", err)
		os.Exit(1)
	}
	if status != 200 {
		fmt.Printf("Token 交换失败 (HTTP %d): %s\n", status, string(body))
		os.Exit(1)
	}

	var tokenResp struct {
		AccessToken  string `json:"access_token"`
		RefreshToken string `json:"refresh_token"`
		IDToken      string `json:"id_token"`
		ExpiresIn    int    `json:"expires_in"`
	}
	if err := json.Unmarshal(body, &tokenResp); err != nil {
		fmt.Printf("解析 token 响应失败: %s\n", err)
		os.Exit(1)
	}

	email := ""
	accountID := ""
	if tokenResp.IDToken != "" {
		claims, err := parseJWTPayload(tokenResp.IDToken)
		if err == nil {
			if e, ok := claims["email"].(string); ok {
				email = e
			}
			if auth, ok := claims["https://api.openai.com/auth"].(map[string]interface{}); ok {
				if aid, ok := auth["chatgpt_account_id"].(string); ok {
					accountID = aid
				}
			}
		}
	}

	expIn := tokenResp.ExpiresIn
	if expIn <= 0 {
		expIn = 3600
	}
	expireAt := time.Now().Add(time.Duration(expIn) * time.Second)
	authData := map[string]interface{}{
		"type":          "codex",
		"id_token":      tokenResp.IDToken,
		"access_token":  tokenResp.AccessToken,
		"refresh_token": tokenResp.RefreshToken,
		"account_id":    accountID,
		"email":         email,
		"last_refresh":  time.Now().Format(time.RFC3339Nano),
		"expired":       expireAt.Format(time.RFC3339Nano),
	}

	if err := writeAccountFile("codex_account.json", authData); err != nil {
		fmt.Printf("保存凭据失败: %s\n", err)
		os.Exit(1)
	}

	fmt.Println()
	fmt.Println("✓ 认证成功!")
	if email != "" {
		fmt.Printf("  邮箱: %s\n", email)
	}
	fmt.Println("  凭据已保存到: auths/codex_account.json")
}

// ── Gemini login ────────────────────────────────────────────────────────────

func loginGemini() {
	fmt.Println("初始化 Gemini 登录...")

	state, err := generateRandomState()
	if err != nil {
		fmt.Printf("生成 state 失败: %s\n", err)
		os.Exit(1)
	}

	params := url.Values{
		"access_type":            {"offline"},
		"client_id":              {geminiOAuthClientID},
		"prompt":                 {"consent"},
		"redirect_uri":           {geminiOAuthRedirect},
		"response_type":          {"code"},
		"scope":                  {strings.Join(geminiOAuthScopes, " ")},
		"state":                  {state},
		"include_granted_scopes": {"true"},
	}
	authURL := geminiOAuthAuthURL + "?" + params.Encode()

	fmt.Println()
	fmt.Println("请复制以下链接到浏览器打开并登录:")
	fmt.Println()
	fmt.Println("  " + authURL)

	rawCallback := promptCallbackURL()
	code, cbState, err := parseCallbackURL(rawCallback)
	if err != nil {
		fmt.Printf("解析回调 URL 失败: %s\n", err)
		os.Exit(1)
	}
	if cbState != state {
		fmt.Printf("state 不匹配\n")
		os.Exit(1)
	}

	fmt.Println("正在交换 token...")
	form := url.Values{
		"grant_type":    {"authorization_code"},
		"client_id":     {geminiOAuthClientID},
		"client_secret": {geminiOAuthClientS},
		"code":          {code},
		"redirect_uri":  {geminiOAuthRedirect},
	}
	body, status, err := doOAuthPOST(agTokenURL, form)
	if err != nil {
		fmt.Printf("Token 交换请求失败: %s\n", err)
		os.Exit(1)
	}
	if status != 200 {
		fmt.Printf("Token 交换失败 (HTTP %d): %s\n", status, string(body))
		os.Exit(1)
	}

	var tokenResp struct {
		AccessToken  string `json:"access_token"`
		RefreshToken string `json:"refresh_token"`
		ExpiresIn    int64  `json:"expires_in"`
		TokenType    string `json:"token_type"`
	}
	if err := json.Unmarshal(body, &tokenResp); err != nil {
		fmt.Printf("解析 token 响应失败: %s\n", err)
		os.Exit(1)
	}

	email := ""
	if tokenResp.AccessToken != "" {
		email = fetchGeminiEmail(tokenResp.AccessToken)
	}
	if email == "" {
		fmt.Println("  ⚠ 未能获取邮箱信息")
	}

	projectID := ""
	if tokenResp.AccessToken != "" {
		projectID = fetchGeminiProjectID(tokenResp.AccessToken)
	}
	if projectID == "" {
		fmt.Println("  ⚠ 未能获取项目 ID")
	}

	expIn := tokenResp.ExpiresIn
	if expIn <= 0 {
		expIn = 3600
	}
	expireAt := time.Now().Add(time.Duration(expIn) * time.Second)
	authData := map[string]interface{}{
		"type":          "antigravity",
		"access_token":  tokenResp.AccessToken,
		"refresh_token": tokenResp.RefreshToken,
		"expires_in":    tokenResp.ExpiresIn,
		"timestamp":     time.Now().UnixMilli(),
		"expired":       expireAt.Format(time.RFC3339Nano),
	}
	if email != "" {
		authData["email"] = email
	}
	if projectID != "" {
		authData["project_id"] = projectID
	}

	if err := writeAccountFile("gemini_account.json", authData); err != nil {
		fmt.Printf("保存凭据失败: %s\n", err)
		os.Exit(1)
	}

	fmt.Println()
	fmt.Println("✓ 认证成功!")
	if email != "" {
		fmt.Printf("  邮箱: %s\n", email)
	}
	if projectID != "" {
		fmt.Printf("  项目: %s\n", projectID)
	}
	fmt.Println("  凭据已保存到: auths/gemini_account.json")
}

func fetchGeminiEmail(accessToken string) string {
	req, err := http.NewRequestWithContext(context.Background(), "GET", geminiOAuthUserInfo, nil)
	if err != nil {
		return ""
	}
	req.Header.Set("Authorization", "Bearer "+accessToken)
	resp, err := httpClient.Do(req)
	if err != nil {
		return ""
	}
	defer resp.Body.Close()
	body, _ := io.ReadAll(io.LimitReader(resp.Body, 4096))
	if resp.StatusCode != 200 {
		return ""
	}
	var info struct {
		Email string `json:"email"`
	}
	if err := json.Unmarshal(body, &info); err != nil {
		return ""
	}
	return info.Email
}

func fetchGeminiProjectID(accessToken string) string {
	reqBody := map[string]interface{}{
		"metadata": map[string]string{"ideType": "ANTIGRAVITY"},
	}
	raw, _ := json.Marshal(reqBody)

	apiURL := "https://cloudcode-pa.googleapis.com/v1internal:loadCodeAssist"
	req, err := http.NewRequestWithContext(context.Background(), "POST", apiURL, strings.NewReader(string(raw)))
	if err != nil {
		return ""
	}
	req.Header.Set("Authorization", "Bearer "+accessToken)
	req.Header.Set("Content-Type", "application/json")

	resp, err := httpClient.Do(req)
	if err != nil {
		return ""
	}
	defer resp.Body.Close()
	body, _ := io.ReadAll(io.LimitReader(resp.Body, 8192))
	if resp.StatusCode != 200 {
		return ""
	}

	var data map[string]interface{}
	if err := json.Unmarshal(body, &data); err != nil {
		return ""
	}

	for _, key := range []string{"cloudaicompanionProject", "projectId", "project"} {
		switch v := data[key].(type) {
		case string:
			if strings.TrimSpace(v) != "" {
				return strings.TrimSpace(v)
			}
		case map[string]interface{}:
			if id, ok := v["id"].(string); ok && strings.TrimSpace(id) != "" {
				return strings.TrimSpace(id)
			}
		}
	}
	return ""
}
