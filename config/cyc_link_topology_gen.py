
nodeNum = 16
switchNode = 6
linkNum = 25
nodeNumPerSwitch = 4
bandWidth = 100
bandWidthStr = str(bandWidth) + "Gbps"
delay = 1000
delayStr = str(delay) + "ns"

with open("cyc_traffic_topology.txt", "w") as f:
    line = [nodeNum + switchNode, switchNode, linkNum]
    f.writelines(' '.join(str(x) for x in line) + '\n')
    line = [x for x in range(nodeNum, nodeNum + switchNode)]
    f.writelines(' '.join(str(x) for x in line) + '\n')
    index = 0
    for i in range(nodeNum, nodeNum + switchNode):
        for j in range(nodeNumPerSwitch):
            line = [j + index * nodeNumPerSwitch, i, bandWidthStr, delayStr, 0]
            f.writelines(' '.join(str(x) for x in line) + '\n')
        index += 1