//go:build windows

package main

import "fmt"

func serverStart()  { fmt.Println("Windows 不支持 daemon 模式") }
func serverStop()   {}
func serverRestart() {}
func serverStatus()  { fmt.Println("Windows 不支持 daemon 模式") }
func isRunning() bool { return false }
