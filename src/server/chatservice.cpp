#include "chatservice.hpp"
#include "public.hpp"
#include <muduo/base/Logging.h>
#include <string>
#include <vector>
using namespace std;
using namespace muduo;
//获取单例对象的接口函数
ChatService* ChatService::instance(){
    static ChatService service;
    return &service;
}

//注册消息以及对应的处理回调操作
ChatService::ChatService(){
    _msgHandlerMap.insert({LOGIN_MSG,std::bind(&ChatService::login,this,_1,_2,_3)});
    _msgHandlerMap.insert({LOGINOUT_MSG,std::bind(&ChatService::loginout,this,_1,_2,_3)});
    _msgHandlerMap.insert({REG_MSG,std::bind(&ChatService::reg,this,_1,_2,_3)});
    _msgHandlerMap.insert({ONE_CHAT_MSG,std::bind(&ChatService::oneChat,this,_1,_2,_3)});
    _msgHandlerMap.insert({ADD_FRIEND_MSG,std::bind(&ChatService::addFriend,this,_1,_2,_3)});

    // 群组业务管理相关事件处理回调注册
    _msgHandlerMap.insert({CREATE_GROUP_MSG, std::bind(&ChatService::createGroup, this, _1, _2, _3)});
    _msgHandlerMap.insert({ADD_GROUP_MSG, std::bind(&ChatService::addGroup, this, _1, _2, _3)});
    _msgHandlerMap.insert({GROUP_CHAT_MSG, std::bind(&ChatService::groupChat, this, _1, _2, _3)});

    // 连接redis服务器
    if (_redis.connect()){
        // 设置上报消息的回调
        _redis.init_notify_handler(std::bind(&ChatService::handleRedisSubscribeMessage, this, _1, _2));
    }
}


//处理登录业务  ORM   id password
void ChatService::login(const TcpConnectionPtr& conn,json &js,Timestamp time){
    //LOG_INFO<<"do login service!";
    //printf("do login service!\n");
    int id=js["id"];
    string pwd=js["password"];
    User user=_userModel.query(id);
    if(user.getId()==id&&user.getPwd()==pwd){
        if(user.getState()=="online"){
            //该用户已经登录，不允许重复登录
            json response;
            response["msgid"]=LOGIN_MSG_ACK;
            response["errno"]=2;
            response["errmsg"]="该用户已经登陆";
            conn->send(response.dump());
        }else{
            //登录成功、记录用户连接信息
            {
                lock_guard<mutex> lock(_connMutex);
                _userConnMap.insert({id,conn});
            }
            //id用户登录成功后，向redis订阅channel(id)
            _redis.subscribe(id);

            //登录成功、更新用户状态信息online
            user.setState("online");
            _userModel.updateState(user);

            json response;
            response["msgid"]=LOGIN_MSG_ACK;
            response["errno"]=0;
            response["id"]=user.getId();
            response["name"]=user.getName();
            //查询该用户是否有离线消息
            vector<string> vec=_offlineMsgModel.query(id);
            if(!vec.empty()){
                response["offlinemsg"]=vec;
                _offlineMsgModel.remove(id);
            }

            //查询用户的好友信息并返回
            vector<User> userVec=_friendModel.query(id);
            if(!userVec.empty()){
                //vector<User>不行，json序列化不支持,需要转换成vector<string>
                //response["friends"]=userVec;
                vector<string> vec2;
                for(auto &user:userVec){
                    json js;
                    js["id"]=user.getId();
                    js["name"]=user.getName();
                    js["state"]=user.getState();
                    vec2.push_back(js.dump());
                }
                response["friends"]=vec2;
            }
            conn->send(response.dump());
        }
        
    }else{
        //登录失败
        json response;
        response["msgid"]=LOGIN_MSG_ACK;
        response["errno"]=1;
        response["errmsg"]="用户名或密码错误";
        conn->send(response.dump());
    }

}



//处理注册业务 name password
void ChatService::reg(const TcpConnectionPtr& conn,json &js,Timestamp time){
    //LOG_INFO<<"do reg service!";
    //printf("do reg service!\n");
    string name=js["name"];
    string pwd=js["password"];

    User user;
    user.setName(name);
    user.setPwd(pwd);
    bool state=_userModel.insert(user);
    if(state){
        //注册成功
        json response;
        response["msgid"]=REG_MSG_ACK;
        response["errno"]=0;
        response["id"]=user.getId();
        conn->send(response.dump());
    }else{
        //注册失败
        json response;
        response["msgid"]=REG_MSG_ACK;
        response["errno"]=1;
        conn->send(response.dump());
    }
}

MsgHandler ChatService::getHandler(int msgid){
    //记录错误日志，msgid没有对应的事件处理回调
    auto it=_msgHandlerMap.find(msgid);
    if(it==_msgHandlerMap.end()){
        // LOG_ERROR<<"msgid: "<<msgid<<" can not find handler!";
        // string errstr="msgid:"+msgid  +" can not find handler!";
        //返回一个默认的处理器，空操作
        return [=](const TcpConnectionPtr& conn,json &js,Timestamp time){
            LOG_ERROR<<"msgid: "<<msgid<<" can not find handler!";
            //printf("do error service!\n");
        };
    }else{
        return _msgHandlerMap[msgid];
    }
}

//处理注销业务
void ChatService::loginout(const TcpConnectionPtr &conn, json &js, Timestamp time){
    int userid=js["id"].get<int>();
    {
        lock_guard<mutex> lock(_connMutex);
        auto it=_userConnMap.find(userid);
        if(it!=_userConnMap.end()){
            _userConnMap.erase(it);
        }
    }
    //用户注销，相当于就是下线，在redis中取消订阅通道
    _redis.unsubscribe(userid);

    //更新用户状态信息
    User user(userid,"","","offline");
    _userModel.updateState(user);
}

//处理客户端异常退出，查map表，改数据库offline
void ChatService::clientCloseException(const TcpConnectionPtr &conn){
    User user;
    {
        lock_guard<mutex> lock(_connMutex);
        
        for(auto it=_userConnMap.begin();it!=_userConnMap.end();it++){
            if(it->second==conn){
                user.setId(it->first);
                _userConnMap.erase(it);
                break;
            }
        }
    }
    //用户注销，相当于就是下线，在redis中取消订阅通道
    _redis.unsubscribe(user.getId());

    //更新用户状态信息
    if(user.getId()!=-1){
        user.setState("offline");
        _userModel.updateState(user);
    }

}

//toid msg fromid fromname等
void ChatService::oneChat(const TcpConnectionPtr &conn,json &js,Timestamp time){
    int toid=js["toid"].get<int>();

    {
        lock_guard<mutex> lock(_connMutex);
        auto it=_userConnMap.find(toid);
        if(it!=_userConnMap.end()){
            //toid在线，转发消息
            it->second->send(js.dump());

            return;
        }
    }
    //查询toid是否在线
    User user=_userModel.query(toid);
    if(user.getState()=="online"){
        _redis.publish(toid,js.dump());
        return;
    }

    //toid不在线，存储离线消息
    _offlineMsgModel.insert(toid,js.dump());

}

//服务器异常，业务重置方法
void ChatService::reset(){
    //把所有的用户状态信息重置为offline
    _userModel.resetState();
}

//msgid id friendid
void ChatService::addFriend(const TcpConnectionPtr &conn,json &js,Timestamp time){
    int userid=js["id"].get<int>();
    int friendid=js["friendid"].get<int>();

    //存储好友信息
    _friendModel.insert(userid,friendid);
}

// 创建群组业务
void ChatService::createGroup(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    string name = js["groupname"];
    string desc = js["groupdesc"];

    // 存储新创建的群组信息
    Group group(-1, name, desc);
    if (_groupModel.createGroup(group))
    {
        // 存储群组创建人信息
        _groupModel.addGroup(userid, group.getId(), "creator");
    }
}

// 加入群组业务
void ChatService::addGroup(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
    _groupModel.addGroup(userid, groupid, "normal");
}

// 群组聊天业务
void ChatService::groupChat(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
    vector<int> useridVec = _groupModel.queryGroupUsers(userid, groupid);

    lock_guard<mutex> lock(_connMutex);
    for (int id : useridVec)
    {
        
        auto it = _userConnMap.find(id);
        if (it != _userConnMap.end())
        {
            // 转发群消息
            it->second->send(js.dump());
        }
        else
        {
            // 查询toid是否在线
            User user = _userModel.query(id);
            if (user.getState() == "online"){
                _redis.publish(id, js.dump());
            }else{
                // 存储离线群消息
                _offlineMsgModel.insert(id, js.dump());
            }
            
        
        }
    }
}

// 从redis消息队列中获取订阅的消息
void ChatService::ChatService::handleRedisSubscribeMessage(int userid, string msg)
{
    lock_guard<mutex> lock(_connMutex);
    auto it = _userConnMap.find(userid);
    if (it != _userConnMap.end())
    {
        it->second->send(msg);
        return;
    }

    // 存储该用户的离线消息
    _offlineMsgModel.insert(userid, msg);
}