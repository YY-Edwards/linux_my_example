
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
//��װclient��server
//class Tunnel : public boost::enable_shared_from_this<Tunnel>,
//					  boost::noncopyable
class Tunnel : public std::enable_shared_from_this<Tunnel>,
					  muduo::noncopyable
{
public:

	//�����µ�client
	Tunnel(EventLoop * loop,
		   const InetAddress& serverAddr,
		   const TcpConnectionPtr& serverConn);
	~Tunnel();
	
	//ע�����ӡ���Ϣ�͸�ˮλ�ص�
	void setup();

	//��������Զ�̷����
	void connect();

	//�Ͽ���Զ�̷���˵�����
	void disconnect();

private:


	//ע�����:server�����Ͽ�����;client����/�ͷ���Դ
	void teardown();

	//�ͻ������ӻص�
	void onClientConnection(const TcpConnectionPtr& conn);

	//�ͻ�����Ϣ�ص�
	void onClientMessage(const TcpConnectionPtr& conn,
					     Buffer* buf,
						 Timestamp receiveTime);

	typedef enum
	{
		KServer,
		KClient

	}ObjID;//���еĳ������壬һ����ö�٣���Ϊ���ַ�ʽ�ڱ���ʱ����Ч��
	//��const��ʽ��ֻ�ڶ������������ǳ�����������������ȷʵ�ɱ�ġ�

	//��ˮλ�ص�:�������������ĳ��ȳ������ȷ��ͣ�Ȼ��ʣ���δ���͵����ݳ��ȣ�
	//���û�ָ����Ĭ�ϴ�С��64k��,�ͻᴥ���ص���ֻ�������ش���һ�Σ�
	void onHighWaterMark(ObjID which,
						 const  TcpConnectionPtr& conn,
						 size_t bytesToSent);

	//��ָ�����͵Ļص����Է������ص�ʱ����������/���������͵Ĳ����Ѿ������ڶ������ص�������쳣��
	//������Ϊbind���õ��ǿ����ķ�ʽ�洢����Ͳ���������������ۺܸߡ������޷�������
	//��ô����ʹ��ref�⣬��ref���װ�ɶ�������ã���bind�洢�Ķ������õĿ�����
	//����ע�⣬�������õĻ�����ô�ڲ�ֻҪ�ı��䴫�����ݵ�ֵ�ͻ�ȫ��ͬ���ı䡣

	//������bind��Ա����ʱ��ֱ�Ӵ��ݶ���/���������ǲ���Ҳ�Ǻ�ref��ͬ����Ч����
	//Ч����һ����bindԭ��ֻ֧��ֵ���ݣ���ʹʵ��Ϊ��������Ҳ��Ч��
	//���ԣ�������Ҫ(ֻ֧��ֵ���ݵĺ�������ʱ)��������ʱ�����ʹ��boost::ref.
	//����˵����ref���ǻ������㣬����û��muduo����ʹ�ã��Ǿ���ʱ��Ҫ�á�

	//��̬��Ա���������е�����ĳ�Ա����
	//��̬��Ա����û�����ص�thisָ��
	//��̬��Ա��������ͨ������ֱ�ӷ���
	//��̬��Ա��������ͨ���������
	//��̬��Ա����ֻ��ֱ�ӷ��ʾ�̬��Ա��������������������ֱ�ӷ�����ͨ��Ա������������
	static void onHighWaterMarkWeak(const std::weak_ptr<Tunnel>& wkTunnel,//const boost::weak_ptr<Tunnel>& wkTunnel,
									 ObjID which,
									 const TcpConnectionPtr& conn,
									 size_t bytesToSent);

	//��ˮλ�ص�
	void onWriteComplete(ObjID which, const TcpConnectionPtr& conn);

	//��ָ�����͵Ļص�������ͬ��ˮλ��ָ��ص�
	static void onWriteCompleteWeak(const std::weak_ptr<Tunnel>& wkTunnel,//const boost::weak_ptr<Tunnel>& wkTunnel,
							 ObjID which,
							const TcpConnectionPtr& conn);
private:

	//�洢��ʱ����
	EventLoop*			loop_;
	TcpClient			client_;
	TcpConnectionPtr	serverConn_;//���������ӵ�����˵Ŀͻ�ָ�룺�ⲿ����
	TcpConnectionPtr	clientConn_;//���漴����Զ�̷���˽������ӵĿͻ���ָ�룺�ڲ�����

};


//typedef boost::shared_ptr<Tunnel> TunnelPtr;
typedef std::shared_ptr<Tunnel> TunnelPtr;
#endif  // __TCP_TUNNEL_H