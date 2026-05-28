package main

import (
	"fmt"
	"os"
)

func printHelp() {
	fmt.Print(`多平台模型额度查询工具

用法:
  quota_checker [command] [args]

命令:
  quota_checker                                获取 + 展示全部额度
  quota_checker fetch [gemini|codex|zhipu]     获取额度数据（保存到 cache/）
  quota_checker show [gemini|codex|zhipu]      展示缓存数据
  quota_checker login codex                    Codex (OpenAI) OAuth 登录
  quota_checker login gemini                   Gemini OAuth 登录
  quota_checker server start                   启动 HTTP 服务（后台）
  quota_checker server stop                    停止 HTTP 服务
  quota_checker server restart                 重启 HTTP 服务
  quota_checker server status                  查看服务状态
  quota_checker help                           显示帮助信息

HTTP 服务:
  启动后监听 0.0.0.0:{port}
  接口: GET /LimiT/quErY?provider=gemini|codex|zhipu

配置文件: .config.json
`)
}

func main() {
	ensureRuntimeDirs()
	LoadConfig()
	ensureAccountTemplates()

	args := os.Args[1:]

	// --daemon: 由 server start 拉起的子进程
	if len(args) == 1 && args[0] == "--daemon" {
		runHTTPServer()
		return
	}

	if len(args) == 0 {
		doFetch("all")
		doShow("all")
		return
	}

	switch args[0] {
	case "help", "-h", "--help":
		printHelp()

	case "fetch":
		target := "all"
		if len(args) > 1 {
			target = args[1]
		}
		doFetch(target)

	case "show":
		target := "all"
		if len(args) > 1 {
			target = args[1]
		}
		doShow(target)

	case "login":
		runLogin(args[1:])

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
			fmt.Println("用法: quota_checker server start|stop|restart|status")
		}

	default:
		fmt.Printf("未知命令: %s\n运行 'quota_checker help' 查看帮助\n", args[0])
		os.Exit(1)
	}
}
