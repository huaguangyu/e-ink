package main

import (
	"database/sql"
	"encoding/json"
	"errors"
	"fmt"
	"time"

	_ "github.com/mattn/go-sqlite3"
)

type Store struct {
	db *sql.DB
}

func openStore(path string) (*Store, error) {
	db, err := sql.Open("sqlite3", path+"?_busy_timeout=5000&_journal_mode=WAL")
	if err != nil {
		return nil, err
	}
	db.SetMaxOpenConns(1)

	s := &Store{db: db}
	if err := s.migrate(); err != nil {
		db.Close()
		return nil, err
	}
	return s, nil
}

func (s *Store) Close() error {
	return s.db.Close()
}

func (s *Store) migrate() error {
	stmts := []string{
		`CREATE TABLE IF NOT EXISTS heartbeats (
			id INTEGER PRIMARY KEY AUTOINCREMENT,
			mac TEXT NOT NULL,
			voltage REAL,
			pct INTEGER,
			ip TEXT,
			wake TEXT,
			uptime_s INTEGER,
			device_ts TEXT,
			free_heap INTEGER,
			rssi INTEGER,
			ssid TEXT,
			firmware TEXT,
			sleep_min INTEGER,
			created_at DATETIME DEFAULT CURRENT_TIMESTAMP
		)`,
		`CREATE INDEX IF NOT EXISTS idx_heartbeats_mac_created ON heartbeats(mac, created_at DESC)`,
		`CREATE TABLE IF NOT EXISTS tasks (
			id INTEGER PRIMARY KEY AUTOINCREMENT,
			mac TEXT NOT NULL,
			type TEXT NOT NULL,
			params TEXT,
			sort_order INTEGER DEFAULT 0,
			status TEXT DEFAULT 'pending',
			result TEXT,
			created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
			done_at DATETIME
		)`,
		`CREATE INDEX IF NOT EXISTS idx_tasks_mac_status_order ON tasks(mac, status, sort_order, id)`,
		`CREATE TABLE IF NOT EXISTS sleep_schedules (
			mac         TEXT PRIMARY KEY,
			mode        TEXT    NOT NULL DEFAULT 'fixed',
			fixed_min   INTEGER NOT NULL DEFAULT 5,
			default_min INTEGER NOT NULL DEFAULT 30,
			slots       TEXT    NOT NULL DEFAULT '[]',
			updated_at  DATETIME DEFAULT CURRENT_TIMESTAMP
		)`,
	}
	for _, stmt := range stmts {
		if _, err := s.db.Exec(stmt); err != nil {
			return err
		}
	}
	if err := s.ensureColumn("heartbeats", "ssid", "TEXT"); err != nil {
		return err
	}
	return nil
}

func (s *Store) ensureColumn(table, column, definition string) error {
	rows, err := s.db.Query(`PRAGMA table_info(` + table + `)`)
	if err != nil {
		return err
	}
	defer rows.Close()

	for rows.Next() {
		var cid int
		var name, typ string
		var notNull int
		var defaultValue any
		var pk int
		if err := rows.Scan(&cid, &name, &typ, &notNull, &defaultValue, &pk); err != nil {
			return err
		}
		if name == column {
			return nil
		}
	}
	if err := rows.Err(); err != nil {
		return err
	}
	_, err = s.db.Exec(`ALTER TABLE ` + table + ` ADD COLUMN ` + column + ` ` + definition)
	return err
}

func (s *Store) InsertHeartbeat(h HeartbeatRequest, keep int) (*Heartbeat, error) {
	if h.SleepMin <= 0 {
		h.SleepMin = 5
	}
	res, err := s.db.Exec(`INSERT INTO heartbeats
		(mac, voltage, pct, ip, wake, uptime_s, device_ts, free_heap, rssi, ssid, firmware, sleep_min)
		VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)`,
		h.MAC, h.Voltage, h.Pct, h.IP, h.Wake, h.UptimeS, h.DeviceTS, h.FreeHeap, h.RSSI, h.SSID, h.Firmware, h.SleepMin)
	if err != nil {
		return nil, err
	}
	id, err := res.LastInsertId()
	if err != nil {
		return nil, err
	}
	if err := s.cleanOldHeartbeats(h.MAC, keep); err != nil {
		return nil, err
	}
	return s.GetHeartbeat(id)
}

func (s *Store) cleanOldHeartbeats(mac string, keep int) error {
	if keep <= 0 {
		return nil
	}
	_, err := s.db.Exec(`DELETE FROM heartbeats
		WHERE mac = ? AND id NOT IN (
			SELECT id FROM heartbeats WHERE mac = ? ORDER BY created_at DESC, id DESC LIMIT ?
		)`, mac, mac, keep)
	return err
}

func (s *Store) GetHeartbeat(id int64) (*Heartbeat, error) {
	row := s.db.QueryRow(`SELECT id, mac, voltage, pct, ip, wake, uptime_s, device_ts,
		free_heap, rssi, ssid, firmware, sleep_min, created_at FROM heartbeats WHERE id = ?`, id)
	return scanHeartbeat(row)
}

func (s *Store) LatestHeartbeat() (*Heartbeat, error) {
	row := s.db.QueryRow(`SELECT id, mac, voltage, pct, ip, wake, uptime_s, device_ts,
		free_heap, rssi, ssid, firmware, sleep_min, created_at FROM heartbeats
		ORDER BY created_at DESC, id DESC LIMIT 1`)
	h, err := scanHeartbeat(row)
	if errors.Is(err, sql.ErrNoRows) {
		return nil, nil
	}
	return h, err
}

func (s *Store) ListDevices() ([]DeviceWithStatus, error) {
	rows, err := s.db.Query(`SELECT h.mac, h.voltage, h.pct, h.ip, h.rssi, h.ssid, h.sleep_min, h.created_at
		FROM heartbeats h
		INNER JOIN (
			SELECT mac, MAX(id) AS max_id FROM heartbeats GROUP BY mac
		) latest ON h.id = latest.max_id
		ORDER BY h.created_at DESC`)
	if err != nil {
		return nil, err
	}
	defer rows.Close()

	var list []DeviceWithStatus
	for rows.Next() {
		var d DeviceWithStatus
		var created string
		if err := rows.Scan(&d.MAC, &d.Voltage, &d.Pct, &d.IP, &d.RSSI, &d.SSID, &d.SleepMin, &created); err != nil {
			return nil, err
		}
		d.LastSeen = parseDBTime(created)
		list = append(list, d)
	}
	return list, rows.Err()
}

func (s *Store) ListHeartbeats(mac string, limit int) ([]Heartbeat, error) {
	if limit <= 0 {
		limit = 50
	}
	query := `SELECT id, mac, voltage, pct, ip, wake, uptime_s, device_ts,
		free_heap, rssi, ssid, firmware, sleep_min, created_at FROM heartbeats`
	args := []any{}
	if mac != "" {
		query += ` WHERE mac = ?`
		args = append(args, mac)
	}
	query += ` ORDER BY created_at DESC, id DESC LIMIT ?`
	args = append(args, limit)
	rows, err := s.db.Query(query, args...)
	if err != nil {
		return nil, err
	}
	defer rows.Close()

	var list []Heartbeat
	for rows.Next() {
		h, err := scanHeartbeat(rows)
		if err != nil {
			return nil, err
		}
		list = append(list, *h)
	}
	return list, rows.Err()
}

type heartbeatScanner interface {
	Scan(dest ...any) error
}

func scanHeartbeat(row heartbeatScanner) (*Heartbeat, error) {
	var h Heartbeat
	var created string
	if err := row.Scan(&h.ID, &h.MAC, &h.Voltage, &h.Pct, &h.IP, &h.Wake, &h.UptimeS,
		&h.DeviceTS, &h.FreeHeap, &h.RSSI, &h.SSID, &h.Firmware, &h.SleepMin, &created); err != nil {
		return nil, err
	}
	h.CreatedAt = parseDBTime(created)
	return &h, nil
}

func (s *Store) UpsertPendingTask(req TaskRequest) (*Task, error) {
	if !validTaskType(req.Type) {
		return nil, fmt.Errorf("invalid task type: %s", req.Type)
	}
	params := compactJSON(req.Params)

	tx, err := s.db.Begin()
	if err != nil {
		return nil, err
	}
	defer tx.Rollback()

	var existingID int64
	err = tx.QueryRow(`SELECT id FROM tasks WHERE mac = ? AND type = ? AND status = 'pending'
		ORDER BY id DESC LIMIT 1`, req.MAC, req.Type).Scan(&existingID)
	switch {
	case err == nil:
		if _, err := tx.Exec(`UPDATE tasks SET params = ?, created_at = CURRENT_TIMESTAMP WHERE id = ?`, params, existingID); err != nil {
			return nil, err
		}
	case errors.Is(err, sql.ErrNoRows):
		nextOrder := 0
		if err := tx.QueryRow(`SELECT COALESCE(MAX(sort_order), -1) + 1 FROM tasks WHERE mac = ? AND status = 'pending'`, req.MAC).Scan(&nextOrder); err != nil {
			return nil, err
		}
		res, err := tx.Exec(`INSERT INTO tasks(mac, type, params, sort_order, status)
			VALUES (?, ?, ?, ?, 'pending')`, req.MAC, req.Type, params, nextOrder)
		if err != nil {
			return nil, err
		}
		existingID, err = res.LastInsertId()
		if err != nil {
			return nil, err
		}
	default:
		return nil, err
	}
	if err := tx.Commit(); err != nil {
		return nil, err
	}
	return s.GetTask(existingID)
}

func (s *Store) GetTask(id int64) (*Task, error) {
	row := s.db.QueryRow(`SELECT id, mac, type, params, sort_order, status, result, created_at, done_at
		FROM tasks WHERE id = ?`, id)
	return scanTask(row)
}

func (s *Store) ListTasks(mac, status string, limit int) ([]Task, error) {
	if limit <= 0 {
		limit = 100
	}
	query := `SELECT id, mac, type, params, sort_order, status, result, created_at, done_at FROM tasks`
	args := []any{}
	where := ""
	if mac != "" {
		where = appendWhere(where, "mac = ?")
		args = append(args, mac)
	}
	if status != "" {
		where = appendWhere(where, "status = ?")
		args = append(args, status)
	}
	if where != "" {
		query += " WHERE " + where
	}
	query += ` ORDER BY CASE status WHEN 'pending' THEN 0 WHEN 'sent' THEN 1 WHEN 'failed' THEN 2 WHEN 'done' THEN 3 ELSE 4 END,
		sort_order ASC, id DESC LIMIT ?`
	args = append(args, limit)

	rows, err := s.db.Query(query, args...)
	if err != nil {
		return nil, err
	}
	defer rows.Close()

	var list []Task
	for rows.Next() {
		t, err := scanTask(rows)
		if err != nil {
			return nil, err
		}
		list = append(list, *t)
	}
	return list, rows.Err()
}

func (s *Store) ClaimPendingTasks(mac string) ([]Task, error) {
	tx, err := s.db.Begin()
	if err != nil {
		return nil, err
	}
	defer tx.Rollback()

	rows, err := tx.Query(`SELECT id, mac, type, params, sort_order, status, result, created_at, done_at
		FROM tasks WHERE mac = ? AND status = 'pending' ORDER BY sort_order ASC, id ASC`, mac)
	if err != nil {
		return nil, err
	}
	var tasks []Task
	for rows.Next() {
		t, err := scanTask(rows)
		if err != nil {
			rows.Close()
			return nil, err
		}
		tasks = append(tasks, *t)
	}
	if err := rows.Close(); err != nil {
		return nil, err
	}
	if len(tasks) == 0 {
		if err := tx.Commit(); err != nil {
			return nil, err
		}
		return []Task{}, nil
	}

	for _, t := range tasks {
		if _, err := tx.Exec(`UPDATE tasks SET status = 'sent' WHERE id = ? AND status = 'pending'`, t.ID); err != nil {
			return nil, err
		}
	}
	if err := tx.Commit(); err != nil {
		return nil, err
	}
	for i := range tasks {
		tasks[i].Status = StatusSent
	}
	return tasks, nil
}

func (s *Store) FinishTask(id int64, success bool, data json.RawMessage) (*Task, error) {
	status := StatusFailed
	if success {
		status = StatusDone
	}
	result := compactJSON(data)
	if result == "" {
		result = "{}"
	}
	res, err := s.db.Exec(`UPDATE tasks SET status = ?, result = ?, done_at = CURRENT_TIMESTAMP
		WHERE id = ? AND status IN ('pending', 'sent')`, status, result, id)
	if err != nil {
		return nil, err
	}
	n, err := res.RowsAffected()
	if err != nil {
		return nil, err
	}
	if n == 0 {
		return nil, sql.ErrNoRows
	}
	return s.GetTask(id)
}

func (s *Store) DeleteTask(id int64) error {
	res, err := s.db.Exec(`DELETE FROM tasks WHERE id = ?`, id)
	if err != nil {
		return err
	}
	n, err := res.RowsAffected()
	if err != nil {
		return err
	}
	if n == 0 {
		return sql.ErrNoRows
	}
	return nil
}

func (s *Store) SortTasks(orders []TaskOrder) error {
	tx, err := s.db.Begin()
	if err != nil {
		return err
	}
	defer tx.Rollback()
	for _, order := range orders {
		if _, err := tx.Exec(`UPDATE tasks SET sort_order = ? WHERE id = ? AND status = 'pending'`,
			order.SortOrder, order.ID); err != nil {
			return err
		}
	}
	return tx.Commit()
}

type taskScanner interface {
	Scan(dest ...any) error
}

func scanTask(row taskScanner) (*Task, error) {
	var t Task
	var params, result sql.NullString
	var created string
	var done sql.NullString
	if err := row.Scan(&t.ID, &t.MAC, &t.Type, &params, &t.SortOrder, &t.Status,
		&result, &created, &done); err != nil {
		return nil, err
	}
	t.Params = json.RawMessage("{}")
	if params.Valid && params.String != "" {
		t.Params = json.RawMessage(params.String)
	}
	if result.Valid && result.String != "" {
		t.Result = json.RawMessage(result.String)
	}
	t.CreatedAt = parseDBTime(created)
	if done.Valid && done.String != "" {
		dt := parseDBTime(done.String)
		t.DoneAt = &dt
	}
	return &t, nil
}

func validTaskType(taskType string) bool {
	switch taskType {
	case TaskSetSleep, TaskSyncTime, TaskRestart, TaskSwitchPage:
		return true
	default:
		return false
	}
}

func compactJSON(raw json.RawMessage) string {
	if len(raw) == 0 {
		return "{}"
	}
	var v any
	if err := json.Unmarshal(raw, &v); err != nil {
		return "{}"
	}
	out, err := json.Marshal(v)
	if err != nil {
		return "{}"
	}
	return string(out)
}

func appendWhere(where, next string) string {
	if where == "" {
		return next
	}
	return where + " AND " + next
}

func parseDBTime(v string) time.Time {
	layouts := []string{
		"2006-01-02 15:04:05",
		time.RFC3339Nano,
		time.RFC3339,
	}
	for _, layout := range layouts {
		if t, err := time.ParseInLocation(layout, v, time.Local); err == nil {
			return t
		}
	}
	return time.Time{}
}

func (s *Store) GetSleepSchedule(mac string) (*SleepSchedule, error) {
	row := s.db.QueryRow(`SELECT mac, mode, fixed_min, default_min, slots, updated_at
		FROM sleep_schedules WHERE mac = ?`, mac)
	var sc SleepSchedule
	var slotsStr string
	var updated string
	if err := row.Scan(&sc.MAC, &sc.Mode, &sc.FixedMin, &sc.DefaultMin, &slotsStr, &updated); err != nil {
		if errors.Is(err, sql.ErrNoRows) {
			return nil, nil
		}
		return nil, err
	}
	sc.UpdatedAt = parseDBTime(updated)
	if err := json.Unmarshal([]byte(slotsStr), &sc.Slots); err != nil {
		return nil, err
	}
	return &sc, nil
}

func (s *Store) UpsertSleepSchedule(req ScheduleRequest) (*SleepSchedule, error) {
	slotsJSON, err := json.Marshal(req.Slots)
	if err != nil {
		return nil, err
	}
	_, err = s.db.Exec(`INSERT INTO sleep_schedules (mac, mode, fixed_min, default_min, slots, updated_at)
		VALUES (?, ?, ?, ?, ?, CURRENT_TIMESTAMP)
		ON CONFLICT(mac) DO UPDATE SET mode=excluded.mode, fixed_min=excluded.fixed_min,
		default_min=excluded.default_min, slots=excluded.slots, updated_at=CURRENT_TIMESTAMP`,
		req.MAC, req.Mode, req.FixedMin, req.DefaultMin, string(slotsJSON))
	if err != nil {
		return nil, err
	}
	return s.GetSleepSchedule(req.MAC)
}
