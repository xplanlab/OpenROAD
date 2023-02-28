import json
import os
import time

import zmq

import proto.xroute.net_ordering_pb2 as net_ordering

context = zmq.Context()
socket = context.socket(zmq.REP)
socket.bind("tcp://*:5555")


while True:
    message_raw = socket.recv()

    # 解析消息
    message = net_ordering.Message()
    message.ParseFromString(message_raw)

    if message.HasField('request'):
        req = message.request

        # 找时间和期元统一数据格式

        data = [
            [req.dim_x, req.dim_y, req.dim_z],
            [],
            [req.reward_violation, req.reward_wire_length, req.reward_via],
        ]

        for node in req.nodes:
            node_type = 0
            if node.type == net_ordering.ACCESS:
                node_type = node.net + 1  # net 是从 1 开始的
            elif node.type == net_ordering.BLOCKAGE:
                node_type = -1

            node_pin = -1
            if node.type == net_ordering.NodeType.ACCESS:
                node_pin = node.pin + 1  # pin 是从 1 开始的

            info = [
                [node.maze_x, node.maze_y, node.maze_z],
                [node.point_x, node.point_y, node.point_z],
                [int(node.is_used), node_type, node_pin],
            ]

            data[1].append(info)

        # dataDir = 'drIter'
        # if not os.path.exists(dataDir):
        #     os.makedirs(dataDir)
        #
        # with open(f'{dataDir}/grid_graph_{round(time.time() * 1000)}.json', 'w') as f:
        #     f.write(json.dumps(data, separators=[',', ':']))

        netIndex = 1
        is_input = False
        while not is_input:
            input_str = input('Input a net index: ')

            # 校验
            if not input_str.isdigit():
                print('net index must be a number.')
            elif int(input_str) <= 0:
                print('net index must be greater than 0.')
            else:
                netIndex = int(input_str)
                is_input = True

        # 回复 netIndex
        res = net_ordering.Message()
        res.response.net_index = netIndex - 1  # net 是从 1 开始的，所以返回时记得减回去
        socket.send(res.SerializeToString())

    elif message.HasField('done'):
        print(
            f'Done with violation {message.done.reward_violation}, wireLength {message.done.reward_wire_length}, via {message.done.reward_via}')
        socket.send(b'\0')
    else:
        print('Unknown message type.')
