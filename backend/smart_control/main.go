package main

import (
	"database/sql"
	"encoding/json"
	"errors"
	"fmt"
	"log"
	"net/http"
	"os"
	"path/filepath"
	"strconv"
	"strings"
	"time"

	"github.com/gin-gonic/gin"
)

type Server struct {
	cfg    Config
	store  *Store
	hub    *Hub
	client *http.Client
}

func main() {
	args := os.Args[1:]
	if len(args) == 1 && args[0] == "--daemon" {
		runHTTPServer()
		return
	}
	if len(args) == 0 {
		runHTTPServer()
		return
	}

	switch args[0] {
	case "help", "-h", "--help":
		printHelp()
	case "server":
		action := ""
		if len(args) > 1 {
			action = args[1]
		}
		switch action {
		case "start":
			serverStart()
		case "stop":
			serverStop()
		case "restart":
			serverRestart()
		case "status":
			serverStatus()
		default:
			fmt.Println("用法: elnk-console server start|stop|restart|status")
		}
	default:
		fmt.Printf("未知命令: %s\n运行 'elnk-console help' 查看帮助\n", args[0])
		os.Exit(1)
	}
}

func printHelp() {
	fmt.Print(`eink 智控台服务

用法:
  elnk-console                         前台启动 HTTP 服务
  elnk-console server start            后台启动 HTTP 服务
  elnk-console server stop             停止 HTTP 服务
  elnk-console server restart          重启 HTTP 服务
  elnk-console server status           查看服务状态
  elnk-console help                    显示帮助信息

默认监听:
  0.0.0.0:9857

环境变量:
  EINK_CONSOLE_ADDR            监听地址，默认 0.0.0.0:9857
  EINK_CONSOLE_DB              SQLite 路径，默认 eink_console.db
  EINK_HTTP_TIMEOUT_SEC        HTTP 超时秒数
  EINK_KEEP_HEARTBEATS         每台设备保留心跳数量
`)
}

func runHTTPServer() {
	if err := os.Chdir(appBaseDir()); err != nil {
		log.Fatalf("chdir failed: %v", err)
	}

	cfg := loadConfig()
	store, err := openStore(cfg.DBPath)
	if err != nil {
		log.Fatalf("open db failed: %v", err)
	}
	defer store.Close()

	s := &Server{
		cfg:   cfg,
		store: store,
		hub:   NewHub(),
		client: &http.Client{
			Timeout: cfg.HTTPTimeout,
		},
	}

	router := gin.Default()
	router.GET("/", s.handleIndex)
	router.GET("/health", func(c *gin.Context) {
		c.JSON(http.StatusOK, gin.H{"ok": true})
	})
	router.GET("/ws", s.hub.Handle)

	api := router.Group("/api")
	api.POST("/heartbeat", s.handleHeartbeat)
	api.GET("/tasks", s.handleTasks)
	api.POST("/tasks", s.handleCreateTask)
	api.PUT("/tasks/sort", s.handleSortTasks)
	api.DELETE("/tasks/:id", s.handleDeleteTask)
	api.POST("/tasks/:id/result", s.handleTaskResult)
	api.GET("/status", s.handleStatus)
	api.GET("/devices", s.handleDevices)
	api.GET("/schedule", s.handleGetSchedule)
	api.PUT("/schedule", s.handlePutSchedule)

	router.Static("/static", "./static")

	log.Printf("eink-console listening on %s", cfg.Addr)
	if err := router.Run(cfg.Addr); err != nil {
		log.Fatal(err)
	}
}

func appBaseDir() string {
	wd, _ := os.Getwd()
	if _, err := os.Stat(filepath.Join(wd, "static", "index.html")); err == nil {
		return wd
	}
	exe, err := os.Executable()
	if err != nil {
		return wd
	}
	return filepath.Dir(exe)
}

func (s *Server) handleIndex(c *gin.Context) {
	c.File(filepath.Join("static", "index.html"))
}

func (s *Server) handleHeartbeat(c *gin.Context) {
	var req HeartbeatRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}
	req.MAC = normalizeMAC(req.MAC)
	if req.MAC == "" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "mac is required"})
		return
	}

	hb, err := s.store.InsertHeartbeat(req, s.cfg.KeepBeats)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}
	s.hub.Broadcast("heartbeat", hb)

	schedule, _ := s.store.GetSleepSchedule(req.MAC)
	sleepMin := calcSleepMin(schedule, time.Now())
	if sleepMin <= 0 {
		sleepMin = 5
	}
	persist := schedule == nil || schedule.Mode == "fixed"
	c.JSON(http.StatusOK, HeartbeatResponse{
		SleepMin: sleepMin,
		Persist:  persist,
		Restart:  false,
	})
}

func (s *Server) handleTasks(c *gin.Context) {
	mac := normalizeMAC(c.Query("mac"))
	status := c.Query("status")
	limit := queryInt(c, "limit", 100)

	// Device path: GET /api/tasks?mac=...&claim=1, or default claim when no status is requested.
	if mac != "" && status == "" && c.Query("history") != "1" {
		tasks, err := s.store.ClaimPendingTasks(mac)
		if err != nil {
			c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
			return
		}
		if len(tasks) > 0 {
			s.hub.Broadcast("tasks_claimed", gin.H{"mac": mac, "tasks": tasks})
		}
		c.JSON(http.StatusOK, gin.H{"tasks": tasks})
		return
	}

	tasks, err := s.store.ListTasks(mac, status, limit)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}
	c.JSON(http.StatusOK, gin.H{"tasks": tasks})
}

func (s *Server) handleCreateTask(c *gin.Context) {
	var req TaskRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}
	req.MAC = normalizeMAC(req.MAC)
	if req.MAC == "" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "mac is required"})
		return
	}

	task, err := s.store.UpsertPendingTask(req)
	if err != nil {
		code := http.StatusInternalServerError
		if strings.HasPrefix(err.Error(), "invalid task type") {
			code = http.StatusBadRequest
		}
		c.JSON(code, gin.H{"error": err.Error()})
		return
	}
	s.hub.Broadcast("task_changed", task)
	c.JSON(http.StatusOK, task)
}

func (s *Server) handleTaskResult(c *gin.Context) {
	id, ok := parseIDParam(c)
	if !ok {
		return
	}
	var req TaskResultRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}
	task, err := s.store.FinishTask(id, req.Success, req.Data)
	if err != nil {
		if errors.Is(err, sql.ErrNoRows) {
			c.JSON(http.StatusNotFound, gin.H{"error": "task not found or already finished"})
			return
		}
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}
	s.hub.Broadcast("task_result", task)
	c.JSON(http.StatusOK, gin.H{"ok": true})
}

func (s *Server) handleDeleteTask(c *gin.Context) {
	id, ok := parseIDParam(c)
	if !ok {
		return
	}
	if err := s.store.DeleteTask(id); err != nil {
		if errors.Is(err, sql.ErrNoRows) {
			c.JSON(http.StatusNotFound, gin.H{"error": "task not found"})
			return
		}
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}
	s.hub.Broadcast("task_deleted", gin.H{"id": id})
	c.JSON(http.StatusOK, gin.H{"ok": true})
}

func (s *Server) handleSortTasks(c *gin.Context) {
	var req SortRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}
	if err := s.store.SortTasks(req.Orders); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}
	s.hub.Broadcast("tasks_sorted", req)
	c.JSON(http.StatusOK, gin.H{"ok": true})
}

func (s *Server) handleStatus(c *gin.Context) {
	mac := normalizeMAC(c.Query("mac"))
	limit := queryInt(c, "limit", 20)

	devices, err := s.store.ListDevices()
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}

	if mac == "" {
		latest, err := s.store.LatestHeartbeat()
		if err != nil {
			c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
			return
		}
		if latest != nil {
			mac = latest.MAC
		}
	}

	beats, err := s.store.ListHeartbeats(mac, limit)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}
	tasks, err := s.store.ListTasks(mac, "", 100)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}
	var latest *Heartbeat
	if len(beats) > 0 {
		latest = &beats[0]
	}
	var schedule *SleepSchedule
	if mac != "" {
		schedule, _ = s.store.GetSleepSchedule(mac)
	}
	c.JSON(http.StatusOK, StatusResponse{
		Latest:     latest,
		Heartbeats: beats,
		Tasks:      tasks,
		Devices:    devices,
		Schedule:   schedule,
	})
}

func (s *Server) handleDevices(c *gin.Context) {
	devices, err := s.store.ListDevices()
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}
	c.JSON(http.StatusOK, gin.H{"devices": devices})
}

func parseIDParam(c *gin.Context) (int64, bool) {
	id, err := strconv.ParseInt(c.Param("id"), 10, 64)
	if err != nil || id <= 0 {
		c.JSON(http.StatusBadRequest, gin.H{"error": "invalid id"})
		return 0, false
	}
	return id, true
}

func queryInt(c *gin.Context, key string, fallback int) int {
	v := c.Query(key)
	if v == "" {
		return fallback
	}
	n, err := strconv.Atoi(v)
	if err != nil || n <= 0 {
		return fallback
	}
	return n
}

func normalizeMAC(mac string) string {
	mac = strings.TrimSpace(mac)
	if mac == "" {
		return ""
	}
	mac = strings.ReplaceAll(mac, ":", "")
	mac = strings.ReplaceAll(mac, "-", "")
	return strings.ToUpper(mac)
}

func prettyJSON(raw json.RawMessage) string {
	if len(raw) == 0 {
		return "{}"
	}
	var v any
	if err := json.Unmarshal(raw, &v); err != nil {
		return string(raw)
	}
	out, err := json.MarshalIndent(v, "", "  ")
	if err != nil {
		return string(raw)
	}
	return string(out)
}
