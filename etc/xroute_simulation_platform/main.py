# coding=utf-8

import select
import subprocess
import sys
import threading
import time

import zmq

import proto.xroute.net_ordering_pb2 as net_ordering

process = None


def player_init():
    while True:
        # 启动程序
        global process
        process = subprocess.Popen([f'../../{cmakeDir}/src/openroad', '-exit',
                                    '/home/plan/eda/OpenROAD/src/drt/test/results/ispd18_sample2/run-debug.tcl'],
                                   # '/Users/matts8023/Home/Career/SYSU/eda/OpenROAD/src/drt/test/results/ispd18_sample2/run-debug.tcl'],
                                   cwd=f'../../{cmakeDir}/src/',
                                   stdout=subprocess.PIPE)

        # 创建一个 select 对象，将程序的输出文件描述符添加到其中
        select_obj = select.poll()
        select_obj.register(process.stdout, select.POLLIN)

        while True:
            # 等待文件描述符就绪
            ready = select_obj.poll(1000)  # 等待 1000 毫秒

            if ready:
                # 读取程序的输出
                output = process.stdout.readline().decode().strip()

                # 处理程序的输出
                print(output)

            # 检查程序是否已经返回
            if process.poll() is not None:
                break

        # 等待一段时间再次启动程序
        time.sleep(1)


if len(sys.argv) > 1 and sys.argv[1] == 'server':
    cmakeDir = 'cmake-build-c04-default'
else:
    cmakeDir = 'cmake-build-system-default'

context = zmq.Context()
socket = context.socket(zmq.REP)
socket.bind('tcp://*:6666')

while True:
    msg = socket.recv()
    socket.send(b'OK')

    if process:
        print("\033[31m%s\033[0m" % "Restart OpenROAD")
        process.kill()

    t = threading.Thread(target=player_init)
    t.start()
