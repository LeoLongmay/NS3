

nodeNum = 512
switchNum = 64
linkNum = 1536
bandwidth = 100
delay = 1000
bandwidthStr = str(bandwidth) + "Gps"
delayStr = str(delay) + "ns"
halfSwitchNum = int(switchNum / 2)
linkNumPerNode = 16

with open("leaf_spine_{}_{}G_OS2.txt".format(nodeNum, bandwidth), "w") as f:
    line = [nodeNum + switchNum, switchNum, linkNum]
    f.writelines(' '.join(str(x) for x in line) + "\n")
    line = [x for x in range(nodeNum, nodeNum + switchNum)]
    f.writelines(' '.join(str(x) for x in line) + '\n')
    index = 0
    for i in range(nodeNum, nodeNum + halfSwitchNum):
        for j in range(linkNumPerNode):
            line = [j + index * linkNumPerNode, i, bandwidthStr, delayStr, 0]
            f.writelines(' '.join(str(x) for x in line) + "\n")
        index += 1
    
    for i in range(nodeNum, nodeNum + halfSwitchNum):
        for j in range(nodeNum + halfSwitchNum, nodeNum + switchNum):
            line = [i, j, bandwidthStr, delayStr, 0]
            f.writelines(' '.join(str(x) for x in line) + '\n')