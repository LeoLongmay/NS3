import sys
import random
import math
import heapq
from optparse import OptionParser
from custom_rand import CustomRand
from jsonTotxt import jsonRsp

# 定义网络流量：flow
# init：初始化实例，src：源主机，dst:目的主机，size:流量大小，t:流量发生的时间
class Flow:
	def __init__(self, src, dst, size, t):
		self.src, self.dst, self.size, self.t = src, dst, size, t
	def __str__(self):
		return "%d %d 3 %d %.9f"%(self.src, self.dst, self.size, self.t)

# 带宽转换函数：将带宽字符串如G或M转换为 bit 比特
def translate_bandwidth(b):
	if b == None:
		return None
	if type(b)!=str:
		return None
	if b[-1] == 'G':
		return float(b[:-1])*1e9
	if b[-1] == 'M':
		return float(b[:-1])*1e6
	if b[-1] == 'K':
		return float(b[:-1])*1e3
	return float(b)

# possion函数实现了泊松分布的生成：生成泊松分布的随机值，得到一个符合泊松分布的随机时间间隔。
def poisson(lam):
	return -math.log(1-random.random())*lam

if __name__ == "__main__":
	port = 80
	parser = OptionParser()
	parser.add_option("-c", "--cdf", dest = "cdf_file", help = "the file of the traffic size cdf", default = "uniform_distribution.txt")
	parser.add_option("-n", "--nhost", dest = "nhost", help = "number of hosts")
	parser.add_option("-l", "--load", dest = "load", help = "the percentage of the traffic load to the network capacity, by default 0.3", default = "0.3")
	parser.add_option("-b", "--bandwidth", dest = "bandwidth", help = "the bandwidth of host link (G/M/K), by default 10G", default = "10G")
	parser.add_option("-t", "--time", dest = "time", help = "the total run time (s), by default 10", default = "10")
	parser.add_option("-o", "--output", dest = "output", help = "the output file", default = "tmp_traffic.txt")
	options,args = parser.parse_args()

	linkQueryFlowFilePath = "LLM_data/15-linkflow_report_20250717_102241.json"
	base_t = 2000000000 # 2000000000

	# 参数转换
	if not options.nhost:
		print("please use -n to enter number of hosts")
		sys.exit(0)
	nhost = int(options.nhost)
	load = float(options.load)
	bandwidth = translate_bandwidth(options.bandwidth)
	time = float(options.time)*1e9 # translates to ns
	output = options.output
	if bandwidth == None:
		print("bandwidth format incorrect")
		sys.exit(0)

	fileName = options.cdf_file
	file = open(fileName,"r")
	lines = file.readlines()
	# read the cdf, save in cdf as [[x_i, cdf_i] ...]
	cdf = []
	for line in lines:
		x,y = map(float, line.strip().split(' '))
		cdf.append([x,y])

	# create a custom random generator, which takes a cdf, and generate number according to the cdf
	customRand = CustomRand()
	if not customRand.setCdf(cdf):
		print("Error: Not valid cdf")
		sys.exit(0)

	ofile = open(output, "w")
	# 流信息列表
	flowInfoList = []

	# generate flows
	avg = customRand.getAvg()
	# avg_inter_arrival = 1/(bandwidth*load/8./avg)*1000000000
	# 将流的平均时间间隔增大50倍
	avg_inter_arrival = 1/(bandwidth*load/8./avg)*1000000000 * 50
	n_flow_estimate = int(time / avg_inter_arrival * nhost)
	n_flow = 0
	print("n_flow_estimate: %d",n_flow_estimate)
	ofile.write("%d \n"%n_flow_estimate)
	host_list = [(base_t + int(poisson(avg_inter_arrival)), i) for i in range(nhost)]
	heapq.heapify(host_list)
	while len(host_list) > 0:
		t,src = host_list[0]
		inter_t = int(poisson(avg_inter_arrival))
		new_tuple = (src, t + inter_t)
		dst = random.randint(0, nhost-1)
		while (dst == src):
			dst = random.randint(0, nhost-1)
		if (t + inter_t > time + base_t):
			heapq.heappop(host_list)
		else:
			size = int(customRand.rand())
			if size <= 0:
				size = 1
			n_flow += 1
			# ofile.write("%d %d 3 %d %.9f\n"%(src, dst, size, t * 1e-9))
			flowInfoList.append({'src':str(src), 'dst':str(dst), 'pg':str(3), 'size':str(size), 'start_time':str(t * 1e-9)})
			heapq.heapreplace(host_list, (t + inter_t, src))
	print(f"n_flow_real: {n_flow}")

	try:
		# json数据格式化器
		formatter = jsonRsp(file_path=linkQueryFlowFilePath)
	except Exception as e:
		print(f"Exception {e}")
	# 大模型训练流量开始时间
	largeModelFLowStartTime = 2.0005
	# 大模型训练流量间隔时间
	largeModelFLowInterval = 0.0005
	# 大模型训练流量数据
	largeModelFlowData = formatter.parsed_data['data']['data'] 

	for data in largeModelFlowData:
		nodePair = [node.split('-')[0].split('.')[-1][1:].lstrip('0') for node in data['linkDir'].split("->")]
		# flowSize的单位为GB
		flowSize = float(data['meanSpeed'].split(' ')[0]) * 8
		# 转化为包的个数，包的大小定义在模拟脚本中的参数packet_payload_size里
		packetCount = min(int(flowSize * 1000000000 / 1000), 4294967295)
		largeModelFLowStartTime += largeModelFLowInterval
		flowInfoList.append({'src':nodePair[0], 'dst':nodePair[1], 'pg':str(3), 'size':str(packetCount), 'start_time':"{0:.4f}".format(largeModelFLowStartTime)})
		n_flow += 1

	# 大模型训练流量开始时间
	largeModelFLowStartTime = 2.0001
	# 大模型训练流量间隔时间
	largeModelFLowInterval = 0.0007
	for data in largeModelFlowData:
		nodePair = [str(int(node.split('-')[0].split('.')[-1]) % nhost) for node in data['linkDir'].split("->")]
		# flowSize的单位为GB
		flowSize = float(data['meanSpeed'].split(' ')[0]) * 4
		# 转化为包的个数，包的大小定义在模拟脚本中的参数packet_payload_size里
		packetCount = min(int(flowSize * 1000000000 / 1000), 4294967295)
		largeModelFLowStartTime += largeModelFLowInterval
		flowInfoList.append({'src':nodePair[0], 'dst':nodePair[1], 'pg':str(3), 'size':str(packetCount), 'start_time':"{0:.4f}".format(largeModelFLowStartTime)})
		n_flow += 1
	print(f"n_large_model_flow: {2 * len(largeModelFlowData)}")

	# 按照开始时间进行升序排序
	flowInfoList.sort(key=lambda x: x['start_time'])

	# 将所有流写入文件中
	for flowInfoData in flowInfoList:
		values = [str(v) for v in flowInfoData.values()]
		ofile.write(' '.join(values) + '\n')

	ofile.seek(0)
	ofile.write("%d"%n_flow)
	ofile.close()

	# 1280 + 6182 = 7462

'''
	f_list = []
	avg = customRand.getAvg()
	avg_inter_arrival = 1/(bandwidth*load/8./avg)*1000000000
	# print avg_inter_arrival
	for i in range(nhost):
		t = base_t
		while True:
			inter_t = int(poisson(avg_inter_arrival))
			t += inter_t
			dst = random.randint(0, nhost-1)
			while (dst == i):
				dst = random.randint(0, nhost-1)
			if (t > time + base_t):
				break
			size = int(customRand.rand())
			if size <= 0:
				size = 1
			f_list.append(Flow(i, dst, size, t * 1e-9))

	f_list.sort(key = lambda x: x.t)

	print len(f_list)
	for f in f_list:
		print f
'''
