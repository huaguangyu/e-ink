package main

import (
	"os"
	"strconv"
	"time"
)

type Config struct {
	Addr        string
	DBPath      string
	HTTPTimeout time.Duration
	KeepBeats   int
}

func loadConfig() Config {
	return Config{
		Addr:        envString("EINK_CONSOLE_ADDR", "0.0.0.0:9857"),
		DBPath:      envString("EINK_CONSOLE_DB", "eink_console.db"),
		HTTPTimeout: time.Duration(envInt("EINK_HTTP_TIMEOUT_SEC", 8)) * time.Second,
		KeepBeats:   envInt("EINK_KEEP_HEARTBEATS", 1000),
	}
}

func envString(key, fallback string) string {
	if v := os.Getenv(key); v != "" {
		return v
	}
	return fallback
}

func envInt(key string, fallback int) int {
	if v := os.Getenv(key); v != "" {
		if n, err := strconv.Atoi(v); err == nil {
			return n
		}
	}
	return fallback
}
