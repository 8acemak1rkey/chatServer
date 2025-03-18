# chatServer
基于muduo的可以工作在nginx tcp负载均衡环境中的集群聊天服务器和客户端源码
编译方式
cd build
rm -rf *
cmake ..
make

可执行文件生成于bin目录下

需要nginx的TCP负载均衡
