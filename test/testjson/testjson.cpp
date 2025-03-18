#include "json.hpp"
using json=nlohmann::json;

#include <iostream>
#include <vector>
#include <map>
#include <string>
using namespace std;

string fun1(){
    json js;
    js["msg_type"]=2;
    js["from"]="zhang san";
    js["to"]="li si";
    js["msg"]="hello, what are you doing now?";
    string sendBuf=js.dump();
    // cout<<sendBuf.c_str()<<endl;
    // cout<<js<<endl;
    return sendBuf;
}
string fun2(){
    json js;
    // 添加数组
    js["id"] = {1,2,3,4,5};
    // 添加key-value
    js["name"] = "zhang san";
    // 添加对象
    js["msg"]["zhang san"] = "hello world";
    js["msg"]["liu shuo"] = "hello china";
    // 上面等同于下面这句一次性添加数组对象
    js["msg"] = {{"zhang san", "hello world"}, {"liu shuo", "hello china"}};
    // cout << js << endl;
    return js.dump();
}

//序列化容器代码

void fun3(){
    json js;
    // 直接序列化一个vector容器
    vector<int> vec;
    vec.push_back(1);
    vec.push_back(2);
    vec.push_back(5);
    js["list"] = vec;
    // 直接序列化一个map容器
    map<int, string> m;
    m.insert({1, "黄山"});
    m.insert({2, "华山"});
    m.insert({3, "泰山"});
    js["path"] = m;
    string sendb=js.dump();
    cout<<sendb.c_str()<<endl;
}
int main(){
    string recvBuf=fun2();
    //反序列化    json-->数据对象
    json jsbuf=json::parse(recvBuf);
    // cout<<jsbuf["msg_type"]<<endl;
    // cout<<jsbuf["from"]<<endl;
    cout<<jsbuf["msg"]["zhang san"]<<endl;
    cout<<jsbuf["id"]<<endl;
    auto arr=jsbuf["id"];
    cout<<arr[2]<<endl;

    return 0;
}