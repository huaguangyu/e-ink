//go:build !windows

package main

import (
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"strconv"
	"syscall"
	"time"
)

func pidFile() string {
	return filepath.Join(cacheDir, "app.pid")
}

func readPID() (int, error) {
	data, err := os.ReadFile(pidFile())
	if err != nil {
		return 0, err
	}
	var pid int
	fmt.Sscanf(string(data), "%d", &pid)
	return pid, nil
}

func isRunning() bool {
	pid, err := readPID()
	if err != nil {
		return false
	}
	proc, err := os.FindProcess(pid)
	if err != nil {
		return false
	}
	err = proc.Signal(syscall.Signal(0))
	return err == nil
}

func serverStart() {
	if isRunning() {
		pid, _ := readPID()
		fmt.Printf("服务已在运行中 (PID: %d)\n", pid)
		return
	}
	ensureCacheDir()

	exe, _ := os.Executable()
	cmd := exec.Command(exe, "--daemon")
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr
	cmd.SysProcAttr = &syscall.SysProcAttr{Setpgid: true}
	if err := cmd.Start(); err != nil {
		fmt.Printf("启动失败: %s\n", err)
		os.Exit(1)
	}

	os.WriteFile(pidFile(), []byte(strconv.Itoa(cmd.Process.Pid)), 0644)

	// 等待确认进程存活
	time.Sleep(500 * time.Millisecond)
	if cmd.Process.Signal(syscall.Signal(0)) == nil {
		fmt.Printf("服务已启动 (PID: %d)\n", cmd.Process.Pid)
	} else {
		fmt.Println("服务启动失败")
		os.Remove(pidFile())
		os.Exit(1)
	}
}

func serverStop() {
	pid, err := readPID()
	if err != nil {
		fmt.Println("服务未在运行")
		return
	}
	proc, err := os.FindProcess(pid)
	if err != nil {
		fmt.Println("服务未在运行")
		os.Remove(pidFile())
		return
	}
	if err := proc.Signal(syscall.SIGTERM); err != nil {
		fmt.Println("服务未在运行")
		os.Remove(pidFile())
		return
	}
	// 等待进程退出
	for i := 0; i < 20; i++ {
		time.Sleep(200 * time.Millisecond)
		if proc.Signal(syscall.Signal(0)) != nil {
			fmt.Printf("服务已停止 (PID: %d)\n", pid)
			os.Remove(pidFile())
			return
		}
	}
	proc.Signal(syscall.SIGKILL)
	fmt.Printf("服务已强制停止 (PID: %d)\n", pid)
	os.Remove(pidFile())
}

func serverRestart() {
	if isRunning() {
		serverStop()
	}
	serverStart()
}

func serverStatus() {
	if isRunning() {
		pid, _ := readPID()
		fmt.Printf("服务运行中 (PID: %d)\n", pid)
	} else {
		fmt.Println("服务未运行")
		if _, err := os.Stat(pidFile()); err == nil {
			os.Remove(pidFile())
		}
	}
}
