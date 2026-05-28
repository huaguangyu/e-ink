package main

import (
	"encoding/json"
	"time"
)

const (
	TaskSetSleep   = "set_sleep"
	TaskSyncTime   = "sync_time"
	TaskRestart    = "restart"
	TaskSwitchPage = "switch_page"

	StatusPending = "pending"
	StatusSent    = "sent"
	StatusDone    = "done"
	StatusFailed  = "failed"
)

type HeartbeatRequest struct {
	MAC      string  `json:"mac" binding:"required"`
	Voltage  float64 `json:"voltage"`
	Pct      int     `json:"pct"`
	IP       string  `json:"ip"`
	Wake     string  `json:"wake"`
	UptimeS  int     `json:"uptime_s"`
	DeviceTS string  `json:"device_ts"`
	FreeHeap int     `json:"free_heap"`
	RSSI     int     `json:"rssi"`
	SSID     string  `json:"ssid"`
	Firmware string  `json:"firmware"`
	SleepMin int     `json:"sleep_min"`
}

type Heartbeat struct {
	ID        int64     `json:"id"`
	MAC       string    `json:"mac"`
	Voltage   float64   `json:"voltage"`
	Pct       int       `json:"pct"`
	IP        string    `json:"ip"`
	Wake      string    `json:"wake"`
	UptimeS   int       `json:"uptime_s"`
	DeviceTS  string    `json:"device_ts"`
	FreeHeap  int       `json:"free_heap"`
	RSSI      int       `json:"rssi"`
	SSID      string    `json:"ssid"`
	Firmware  string    `json:"firmware"`
	SleepMin  int       `json:"sleep_min"`
	CreatedAt time.Time `json:"created_at"`
}

type HeartbeatResponse struct {
	SleepMin int  `json:"sleep_min"`
	Persist  bool `json:"persist"`
	Restart  bool `json:"restart"`
}

type TaskRequest struct {
	MAC    string          `json:"mac" binding:"required"`
	Type   string          `json:"type" binding:"required"`
	Params json.RawMessage `json:"params"`
}

type Task struct {
	ID        int64           `json:"id"`
	MAC       string          `json:"mac"`
	Type      string          `json:"type"`
	Params    json.RawMessage `json:"params"`
	SortOrder int             `json:"sort_order"`
	Status    string          `json:"status"`
	Result    json.RawMessage `json:"result,omitempty"`
	CreatedAt time.Time       `json:"created_at"`
	DoneAt    *time.Time      `json:"done_at,omitempty"`
}

type TaskResultRequest struct {
	Success bool            `json:"success"`
	Data    json.RawMessage `json:"data"`
}

type SortRequest struct {
	Orders []TaskOrder `json:"orders"`
}

type TaskOrder struct {
	ID        int64 `json:"id"`
	SortOrder int   `json:"sort_order"`
}

type DeviceWithStatus struct {
	MAC      string    `json:"mac"`
	Voltage  float64   `json:"voltage"`
	Pct      int       `json:"pct"`
	IP       string    `json:"ip"`
	RSSI     int       `json:"rssi"`
	SSID     string    `json:"ssid"`
	SleepMin int       `json:"sleep_min"`
	LastSeen time.Time `json:"last_seen"`
}

type StatusResponse struct {
	Latest     *Heartbeat         `json:"latest"`
	Heartbeats []Heartbeat        `json:"heartbeats"`
	Tasks      []Task             `json:"tasks"`
	Devices    []DeviceWithStatus `json:"devices,omitempty"`
	Schedule   *SleepSchedule     `json:"schedule,omitempty"`
}

type WSEvent struct {
	Type string `json:"type"`
	Data any    `json:"data,omitempty"`
}

type SleepSchedule struct {
	MAC        string    `json:"mac"`
	Mode       string    `json:"mode"`
	FixedMin   int       `json:"fixed_min"`
	DefaultMin int       `json:"default_min"`
	Slots      []Slot    `json:"slots"`
	UpdatedAt  time.Time `json:"updated_at"`
}

type Slot struct {
	Start       string `json:"start"`
	End         string `json:"end"`
	IntervalMin int    `json:"interval_min"`
}

type ScheduleRequest struct {
	MAC        string `json:"mac" binding:"required"`
	Mode       string `json:"mode" binding:"required"`
	FixedMin   int    `json:"fixed_min"`
	DefaultMin int    `json:"default_min"`
	Slots      []Slot `json:"slots"`
}
