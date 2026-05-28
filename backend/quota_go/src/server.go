package main

import (
	"encoding/json"
	"fmt"
	"log"
	"net/http"
	"os"
	"sync"
	"time"

	serverchan_sdk "github.com/easychen/serverchan-sdk-golang"
)

var locCN *time.Location

func init() {
	locCN, _ = time.LoadLocation("Asia/Shanghai")
}

func fmtReset(t time.Time) string {
	now := time.Now()
	tt := t.In(locCN)
	nowCN := now.In(locCN)
	if tt.Year() == nowCN.Year() && tt.YearDay() == nowCN.YearDay() {
		return tt.Format("15:04")
	}
	return tt.Format("01-02")
}

type PermanentError struct {
	Msg string
}

func (e *PermanentError) Error() string { return e.Msg }

type cacheEntry struct {
	Data map[string]interface{}
	Time time.Time
}

var (
	swrCache   = map[string]*cacheEntry{}
	cacheMu    sync.RWMutex
	refreshing = map[string]bool{}
	refreshMu  sync.Mutex
)

func doRefresh(provider string) (map[string]interface{}, error) {
	p := providers[provider]
	raw, err := p.Fetch()
	if err != nil {
		return nil, err
	}
	data := p.Extract(raw)
	data["t"] = time.Now().Unix()
	cacheMu.Lock()
	swrCache[provider] = &cacheEntry{Data: data, Time: time.Now()}
	cacheMu.Unlock()
	ensureCacheDir()
	saveJSON(CachePath(provider), raw)
	return data, nil
}

func loadFromFileCache(provider string) (*cacheEntry, bool) {
	raw, err := loadJSON(CachePath(provider))
	if err != nil {
		return nil, false
	}
	info, err := os.Stat(CachePath(provider))
	if err != nil {
		return nil, false
	}
	p := providers[provider]
	data := p.Extract(raw)
	data["t"] = int64(info.ModTime().Unix())
	entry := &cacheEntry{Data: data, Time: info.ModTime()}
	cacheMu.Lock()
	swrCache[provider] = entry
	cacheMu.Unlock()
	return entry, true
}

func getCached(provider string) (map[string]interface{}, error) {
	ttl := time.Duration(LoadConfig().CacheTTL) * time.Second

	cacheMu.RLock()
	entry, ok := swrCache[provider]
	cacheMu.RUnlock()

	if ok && time.Since(entry.Time) < ttl {
		return entry.Data, nil
	}

	if !ok {
		if e, loaded := loadFromFileCache(provider); loaded && time.Since(e.Time) < ttl {
			return e.Data, nil
		}
	}

	refreshMu.Lock()
	if refreshing[provider] {
		refreshMu.Unlock()
		cacheMu.RLock()
		entry, ok := swrCache[provider]
		cacheMu.RUnlock()
		if ok {
			return entry.Data, nil
		}
		return nil, fmt.Errorf("正在刷新中")
	}
	refreshing[provider] = true
	refreshMu.Unlock()

	data, err := doRefresh(provider)

	refreshMu.Lock()
	delete(refreshing, provider)
	refreshMu.Unlock()
	if err != nil {
		sendNotify(provider, err)
	}
	return data, err
}

func sendNotify(provider string, err error) {
	cfg := LoadConfig()
	if cfg.Notify.SendKey == "" {
		return
	}
	name := providers[provider].Name
	title := fmt.Sprintf("⚠️ 额度查询异常: %s", name)
	body := fmt.Sprintf("📡 平台: %s\n🕐 时间: %s\n❌ 错误: %s",
		name, time.Now().In(locCN).Format("2006-01-02 15:04:05"), err.Error())
	_, sendErr := serverchan_sdk.ScSend(cfg.Notify.SendKey, title, body, nil)
	if sendErr != nil {
		log.Printf("[notify] 发送失败: %s", sendErr)
	}
}

func runHTTPServer() {
	cfg := LoadConfig()
	port := cfg.Port
	path := cfg.ServerPath

	http.HandleFunc(path, func(w http.ResponseWriter, r *http.Request) {
		provider := r.URL.Query().Get("provider")
		w.Header().Set("Content-Type", "application/json")

		// 无参数时返回所有平台列表
		if provider == "" {
			ensureCacheDir()
			var list []map[string]interface{}
				for _, tag := range providerOrder() {
					if IsDisabled(tag) {
						continue
					}
					if tag != "zhipu" {
						if _, err := os.Stat(AccountPath(tag)); os.IsNotExist(err) {
							continue
						}
					}
					data, err := getCached(tag)
				item := map[string]interface{}{"p": tag}
				if err != nil {
					item["error"] = err.Error()
				} else {
					for k, v := range data {
						item[k] = v
					}
				}
				list = append(list, item)
			}
			json.NewEncoder(w).Encode(list)
			return
		}

		if _, ok := providers[provider]; !ok {
			w.WriteHeader(400)
			json.NewEncoder(w).Encode(map[string]interface{}{
				"error":     "invalid provider",
				"providers": providerOrder(),
			})
			return
		}
		if IsDisabled(provider) {
			w.WriteHeader(403)
			json.NewEncoder(w).Encode(map[string]interface{}{
				"error": provider + " is disabled",
			})
			return
		}
		ensureCacheDir()
		data, err := getCached(provider)
		if err != nil {
			w.WriteHeader(500)
			json.NewEncoder(w).Encode(map[string]interface{}{"error": err.Error()})
			return
		}
		json.NewEncoder(w).Encode(data)
	})

	fmt.Printf("启动额度查询服务: http://0.0.0.0:%d%s?provider=gemini|codex|zhipu\n", port, path)
	if err := http.ListenAndServe(fmt.Sprintf(":%d", port), nil); err != nil {
		fmt.Printf("服务启动失败: %s\n", err)
		os.Exit(1)
	}
}
