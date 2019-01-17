
/**
* TCP Tunnel header,Tunnel.h
*
* @platform: linux-4.4.0-62-generic
*
* @author: Edwards
*
* @revision time :20180114
*/

#ifndef __TCP_TUNNEL_H
#define __TCP_TUNNEL_H

#include <muduo/base/Logging.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/TcpServer.h>
#include <muduo/net/TcpClient.h>
#include <muduo/net/InetAddress.h>

//#include <boost/bind.hpp>
//#include <boost/enable_shared_from_this.hpp>


using namespace::muduo;
using namespace::muduo::net;

/*

					<----data---->			proxy               <----data---->
remote_server <-----clientConn_:------- |client/server| <-------:serverConn_------ client




*/
//封装client和server
//class Tunnel : public boost::enable_shared_from_this<Tunnel>,
//					  boost::noncopyable
class Tunnel : public std::enable_shared_from_this<Tunnel>,
					  muduo::noncopyable
{
public:

	//构造新的client
	Tunnel(EventLoop * loop,
		   const InetAddress& serverAddr,
		   const TcpConnectionPtr& serverConn);
	~Tunnel();
	
	//注册连接、消息和高水位回调
	void setup();

	//启动连接远程服务端
	void connect();

	//断开与远程服务端的连接
	void disconnect();

private:


	//注销隧道:server主动断开连接;client重置/释放资源
	void teardown();

	//客户端连接回调
	void onClientConnection(const TcpConnectionPtr& conn);

	//客户端消息回调
	void onClientMessage(const TcpConnectionPtr& conn,
					     Buffer* buf,
						 Timestamp receiveTime);

	typedef enum
	{
		KServer,
		KClient

	}ObjID;//类中的常量定义，一般用枚举，因为此种方式在编译时即生效。
	//而const方式，只在对象生命期内是常量，而对于整个类确实可变的。

	//高水位回调:如果输出缓冲区的长度超过（先发送，然后剩余的未发送的数据长度）
	//了用户指定的默认大小（64k）,就会触发回调（只在上升沿触发一次）
	void onHighWaterMark(ObjID which,
						 const  TcpConnectionPtr& conn,
						 size_t bytesToSent);

	//弱指针类型的回调：以防产生回调时，函数对象/或引用类型的参数已经不存在而继续回调会产生异常。
	//另外因为bind采用的是拷贝的方式存储对象和参数，如果拷贝代价很高、或者无法拷贝，
	//那么可以使用ref库，用ref库包装成对象的引用，即bind存储的对象引用的拷贝。
	//但是注意，传递引用的话，那么内部只要改变其传入数据的值就会全部同样改变。

	//或者在bind成员函数时，直接传递对象/参数引用是不是也是和ref库同样的效果？
	//效果不一样，bind原生只支持值传递，即使实参为引用类型也无效。
	//所以，在有需要(只支持值传递的函数传参时)传递引用时则可以使用boost::ref.
	//有人说引进ref就是画蛇添足，而且没见muduo大量使用，那就暂时不要用。

	//静态成员函数是类中的特殊的成员函数
	//静态成员函数没有隐藏的this指针
	//静态成员函数可以通过类名直接访问
	//静态成员函数可以通过对象访问
	//静态成员函数只能直接访问静态成员变量（函数），而不能直接访问普通成员变量（函数）
	static void onHighWaterMarkWeak(const std::weak_ptr<Tunnel>& wkTunnel,//const boost::weak_ptr<Tunnel>& wkTunnel,
									 ObjID which,
									 const TcpConnectionPtr& conn,
									 size_t bytesToSent);

	//低水位回调
	void onWriteComplete(ObjID which, const TcpConnectionPtr& conn);

	//弱指针类型的回调：理由同高水位弱指针回调
	static void onWriteCompleteWeak(const std::weak_ptr<Tunnel>& wkTunnel,//const boost::weak_ptr<Tunnel>& wkTunnel,
							 ObjID which,
							const TcpConnectionPtr& conn);
private:

	//存储临时变量
	EventLoop*			loop_;
	TcpClient			client_;
	TcpConnectionPtr	serverConn_;//保存已连接到服务端的客户指针：外部传入
	TcpConnectionPtr	clientConn_;//保存即将与远程服务端建立连接的客户端指针：内部构造

};


//typedef boost::shared_ptr<Tunnel> TunnelPtr;
typedef std::shared_ptr<Tunnel> TunnelPtr;
#endif  // __TCP_TUNNEL_H