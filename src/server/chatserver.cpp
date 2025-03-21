#include "chatserver.hpp"
#include "chatservice.hpp"
#include <string>
#include "json.hpp"
using json=nlohmann::json;
using namespace std;
using namespace placeholders;
ChatServer::ChatServer(EventLoop* loop,
                       const InetAddress& listenAddr,
                       const string& nameArg)
    : _server(loop, listenAddr, nameArg), _loop(loop){
        //注册链接回调
        _server.setConnectionCallback(std::bind(&ChatServer::onConnection,this,_1));
        //注册消息回调
        _server.setMessageCallback(std::bind(&ChatServer::onMessage,this,_1,_2,_3));

        //设置线程数量4个线程
        _server.setThreadNum(4);

    }


//启动服务
void ChatServer::start(){
    _server.start();
}

void ChatServer::onConnection(const TcpConnectionPtr& conn){
    if(!conn->connected()){
        //断开连接
        ChatService::instance()->clientCloseException(conn);
        conn->shutdown();
    }    
}

void ChatServer::onMessage(const TcpConnectionPtr& conn,
                           Buffer* buffer,
                           Timestamp time){
    string buf=buffer->retrieveAllAsString();
    //数据的反序列化
    json js=json::parse(buf);
    //达到的目的：完全解耦网络模块的代码和业务模块的代码
    //通过js["msgid"]获取=》业务handler=》conn js time
    auto msgHandler=ChatService::instance()->getHandler(js["msgid"].get<int>());
    msgHandler(conn,js,time);
                           }