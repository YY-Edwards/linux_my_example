
/**
* Codec header,Codec.h
*
* @platform: linux-4.4.0-62-generic
*
* @author: Edwards
*
* @revision time :20180107
*/

#ifndef __MULTIPLEX_CODEC_H
#define __MULTIPLEX_CODEC_H

#include <muduo/base/Atomic.h>
#include <muduo/base/Logging.h>
#include <muduo/net/Buffer.h>
#include <muduo/net/Endian.h>
#include <muduo/net/TcpConnection.h>

#include <boost/function.hpp>
#include <boost/noncopyable.hpp>

using namespace::muduo;
using namespace::muduo::net;


namespace multiplexer
{
	const size_t kMaxPacketLen = 255;
	const size_t kHeaderLen = 3;

	class DescriptorCodec :boost::noncopyable
	{
	public:

		typedef boost::function<void(int id, Buffer* buf)>ClientBufMessageCallback;

		typedef boost::function<void(int id,
			const char* payload,
			int payloadLen)>BackendPayloadCallback;

		explicit DescriptorCodec(const ClientBufMessageCallback& s_cb,
			const BackendPayloadCallback& b_cb)//��ʽ���ù���
			:clientBufMessageCallback_(s_cb)
			, backendPayloadCallback_(b_cb)
		{

		}
		~DescriptorCodec()
		{
		}

		void onClientMessage(const TcpConnectionPtr& conn,
							 Buffer* buf,
							 Timestamp receiveTime)
		{
			size_t packetLen = buf->readableBytes();//��ȡ�յ������ݰ�����
			transferred_.addAndGet(packetLen);//temp +=packetLen;
			receivedMessages_.incrementAndGet();//++numb;
			if (!(conn->getContext().empty()))//�ѽ������ӵ�
			{
				int id = boost::any_cast<int>(conn->getContext());
				//MultiPlexServer::sendBackenBuf
				clientBufMessageCallback_(id, buf);
			}
			else
			{
				LOG_WARN << "Error Connection:[" << conn->localAddress().toIpPort() << "]";
				//buf->retrieveAll();//discard this message
				buf->retrieve(packetLen);
			}
			LOG_DEBUG;
			assert(buf->readableBytes() == 0);//��֤���յ���������ȫ���������
		}
		void onBackendMessage(const TcpConnectionPtr& conn,
							  Buffer* buf,
							  Timestamp receiveTime)
		{
			size_t packetLen = buf->readableBytes();//��ȡ�յ������ݰ�����
			transferred_.addAndGet(packetLen);//temp +=packetLen;
			receivedMessages_.incrementAndGet();//++numb;

			//parse backend message
			while (buf->readableBytes() > kHeaderLen)// kHeaderLen == 3:len(1) + id(2)(little endian)
			{
				int payloadLen = static_cast<uint8_t>(*buf->peek());
				if (buf->readableBytes() < payloadLen + kHeaderLen)
				{
					LOG_DEBUG << "wait for a compelete packet";//���ݲ��������ȴ�ճ��
					break;
				}
				else
				{
					int id = static_cast<uint8_t>(buf->peek()[1]);
					id |= (static_cast<uint8_t>(buf->peek()[2] << 8));
					//����������id���ڲ�ʹ�û����ⲿ�ġ�
					backendPayloadCallback_(id, (buf->peek() + kHeaderLen), payloadLen);//����ͷ��Ϣ��ֱ��ת��
					buf->retrieve(payloadLen + kHeaderLen);//ƫ�ƿɶ�����:len+id+payload
				}
			}
		}

		void sendPacket(const TcpConnectionPtr& conn, int id, Buffer* buf)
		{
			size_t len = buf->readableBytes();
			uint8_t header[kHeaderLen] = {
				static_cast<uint8_t>(len),
				static_cast<uint8_t>(id & 0xFF),
				static_cast<uint8_t>((id & 0xFF00) >> 8),
			};

			buf->prepend(header, kHeaderLen);//��bufԤ���������ͷ��Ϣ��������bufƫ����������ǰ��kHeaderLen��
			conn->send(buf);
		}


		void printStatistics()
		{
			Timestamp endTime = Timestamp::now();
			int64_t newCounter = transferred_.get();
			int64_t bytes = newCounter - oldCounter_;
			int64_t msgs = receivedMessages_.getAndSet(0);
			double time = timeDifference(endTime, startTime_);
			printf("%4.3f MiB/s %4.3f Ki Msgs/s %6.2f bytes per msg\n",
				static_cast<double>(bytes) / time / 1024 / 1024,
				static_cast<double>(msgs) / time / 1024,
				static_cast<double>(bytes) / static_cast<double>(msgs));

			oldCounter_ = newCounter;
			startTime_ = endTime;
		}


	private:


		//���������������ģ�
		AtomicInt64				transferred_;//��������ԭ�Ӳ����ı���
		AtomicInt64				receivedMessages_;
		int64_t					oldCounter_;
		Timestamp				startTime_;

		BackendPayloadCallback		backendPayloadCallback_;
		ClientBufMessageCallback	clientBufMessageCallback_;

	};
}



#endif  // __MULTIPLEX_CODEC_H




