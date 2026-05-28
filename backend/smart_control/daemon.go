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

func runtimeDir() string {
	dir := filepath.Join(appBaseDir(), "run")
	os.MkdirAll(dir, 0755)
	return dir
}

func pidFile() string {
	return filepath.Join(runtimeDir(), "app.pid")
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
	return proc.Signal(syscall.Signal(0)) == nil
}

func serverStart() {
	if isRunning() {
		pid, _ := readPID()
		fmt.Printf("服务已在运行中 (PID: %d)\n", pid)
		return
	}

	exe, err := os.Executable()
	if err != nil {
		fmt.Printf("获取可执行文件失败: %s\n", err)
		os.Exit(1)
	}
	cmd := exec.Command(exe, "--daemon")
	cmd.Dir = appBaseDir()

	logFilePath := filepath.Join(runtimeDir(), "app.log")
	logFile, err := os.OpenFile(logFilePath, os.O_CREATE|os.O_WRONLY|os.O_APPEND, 0644)
	if err == nil {
		cmd.Stdout = logFile
		cmd.Stderr = logFile
	} else {
		cmd.Stdout = nil
		cmd.Stderr = nil
	}

	cmd.SysProcAttr = &syscall.SysProcAttr{Setpgid: true}
	if err := cmd.Start(); err != nil {
		fmt.Printf("启动失败: %s\n", err)
		os.Exit(1)
	}

	os.WriteFile(pidFile(), []byte(strconv.Itoa(cmd.Process.Pid)), 0644)

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
		return
	}
	fmt.Println("服务未运行")
	if _, err := os.Stat(pidFile()); err == nil {
		os.Remove(pidFile())
	}
}
