/**
* Message dispatch header,Dispatcher.h
*
* @platform: linux-4.4.0-62-generic
*
* @author: Edwards
*
* @revision time :20180212
*/

#ifndef __PROTOBUF_CODEC_DISPATCHER_H
#define __PROTOBUF_CODEC_DISPATCHER_H

#include <muduo/base/noncopyable.h>
#include <muduo/net/Callbacks.h>

#include <google/protobuf/message.h>

#include <map>

#include <type_traits>

typedef std::shared_ptr<google::protobuf::Message> MessagePtr;

//dispatcher
//����ֱ�Ӵ����������
class Callback : muduo::noncopyable//���д��麯�������ǳ�����࣬������ӿڣ�����������������Ը��Ǹýӿ�
{

public :

	//1.ÿ���������������� virtual�� ֻ��������Լ��ĳ�Ա��
	//2.�����л���ָ��ָ���ȷ���������Ա������������Ǻ������ģ�
	//	��ô������һ��ָ���������Ա�Ļ���ָ��ʱ���������δ���壨��Ϊ��ʱ�����Ǿ�̬���ࣩ������ֻ�������࣬��������Ȼ���ڡ�
	//  ����� �������� ������ �ڴ�ռ�Ļ���û�н������������������Ϊ�����������������׷����ڴ�й©�¼���
	//	����Ҫ��֤�����ʵ������������������е�������������Ϊ��������

	//ʾ��:
	//C++�л������virtual������������Ϊ�˷�ֹ�ڴ�й©��
	//�����˵��������������������ڴ�ռ䣬���������������ж���Щ�ڴ�ռ�����ͷš�
	//��������в��õ��Ƿ���������������ɾ������ָ��ָ������������ʱ�Ͳ��ᴥ����̬�󶨣����ֻ����û��������������
	//����������������������������ô����������£�������������Ŀռ�͵ò����ͷŴӶ������ڴ�й©��
	//���ԣ�Ϊ�˷�ֹ��������ķ�����C++�л������������Ӧ����virtual������������
	virtual ~Callback() = default;//C++11�е�Ĭ����Ϊ���������������ж�̬��
	virtual void onMessage(const muduo::net::TcpConnectionPtr&,
							const MessagePtr&,
							muduo::Timestamp)const = 0;//����Ϊ���麯��������Ҫ���塣

};

//����һ����ģ��
template <typename T>
class CallbackT : public Callback
{
	//ͨ��std::is_base_of()ȷ������T�����Ǽ̳�����google::protobuf::Message
	static_assert(std::is_base_of<google::protobuf::Message, T>::value,
		"T must be derived from gpb::Message.");

public:

	typedef std::function<void (const muduo::net::TcpConnectionPtr&,
								const std::shared_ptr<T>& message,//����ΪT������ָ�룬��ָ��message
								muduo::Timestamp)> ProtobufMessageTCallback;



	CallbackT(const ProtobufMessageTCallback& callback)
		:callback_(callback)
	{

	}
	~CallbackT() = default;

	void onMessage(const muduo::net::TcpConnectionPtr& conn,
					const MessagePtr& message,
					muduo::Timestamp receiveTime) const override//c++11 ����ȷ��Ҫ��д�ӿڵĹؼ���
	{

		//onMessage -> callback_
		//��Ҫ�õ�����ת�����������ת����
		//���ൽ�����಻������ʽת����
		//�����ൽ���������ת������Ϊÿһ�������඼����һ�����ಿ�֣�
		//����������������һ���֣�Ҳ�п��ܲ��ǣ����Բ����ڻ��ൽ��������Զ�����ת����
		//ר�����shared_ptr���ͣ�Static cast of shared_ptr
		//share_ptrΪ��֧��ת�ͣ������ṩ�����Ƶ�ת�ͺ�����static_pointer_cast<T>��
		//�Ӷ�ʹת�ͺ���ȻΪshared_pointer������Ȼ��ָ����й���
		//��������ڻ����к���һ�������麯�������ǿ���ʹ��dynamic_cast����һ������ת������ת���İ�ȫ��齫������ʱִ�С�
		//�����֪ĳ���������������ת���ǰ�ȫ�ģ������ֱ��ʹ��static_cast��ǿ�Ƹ��ǣ�������Ĳ�����ֻ���������ָ�������ת��
		std::shared_ptr<T> concrete = std::static_pointer_cast<T>(message);
		//std::shared_ptr<T> concrete = muduo::down_pointer_cast<T>(message);
		assert(concrete != NULL);
		callback_(conn, concrete, receiveTime);

	}

private:

	ProtobufMessageTCallback callback_;
};


class ProtobufDispatcher
{
public:

	typedef std::function<void(const muduo::net::TcpConnectionPtr&,
								const MessagePtr&,
								muduo::Timestamp)> ProtobufMessageCallback;


	explicit ProtobufDispatcher(const ProtobufMessageCallback& defaultCb)
		:defaultCallback_(defaultCb)
	{
	}
	~ProtobufDispatcher() = default;

	//����һ��ģ���Ա���������Խ���ע��������Ϣ���͵�T�Ļص���
	//Ȼ��������һ��ģ�廯��������CallbackT<T>,������Ϣ��������Ϣ�ͱ�����CallbackT<T>��
	//registerMessageCallback()��Ĳ�����ʹ�õ�ģ������ĳ�Ա���������൱����Ҫʵ��ģ������ʵ������һ����������ô��Ҫ��typename����CallbackT<T>��һ����
	template<typename T>
	void registerMessageCallback(const typename CallbackT<T>::ProtobufMessageTCallback& callback)
	{
		std::shared_ptr<CallbackT<T>> pd(new CallbackT<T>(callback));//����һ���µ�ģ���࣬�������ϲ�ص�
		//ʵ�ΰ󶨵���������ָ��
		callbacks_[T::descriptor()] = pd;//������Ϣ���ͽ���ָ����ӵ�map��
	}

	void onProtobufMessage(const muduo::net::TcpConnectionPtr& conn,
							const MessagePtr& message,
							muduo::Timestamp receiveTime)const
	{
		CallbackMap::const_iterator it = callbacks_.find(message->GetDescriptor());
		if (it != callbacks_.end())
		{
			//����������û�ָ�����󶨵����Ͳ�ͬ���������ǵ��û���İ汾����������汾
			//��̬��ֻ��������ͨ��ָ������õ����麯��ʱ�Żᷢ��

			//�ر��麯�����ƣ�ʹ�����������ǿ�е��û���İ汾,�����²���
			//it->second->Callback::onMessage(conn, message, receiveTime);
			it->second->onMessage(conn, message, receiveTime);//�ص��������Ϣ���ͣ����ص��û��ص�
		}
		else//���õ�Ĭ�ϻص��������ã�
		{
			defaultCallback_(conn, message, receiveTime);
		}
	
	}

private:

	//�������Ϣ���͵Ļص�map
	//����proto�ļ�ʱ���ѻ�������Ϣ��ʽ���������͡�
	//ÿ���������Ϣ���Ͷ�һ��ȫ�ֵ�Descriptor�������ַ�ǲ���ģ�����������key
	//valueΪ��ͬ�ص�������ָ�룺��̳еĶ�̬��
	//Ϊʲô�ǻ���ָ�룺�����ȿ��԰󶨻������Ҳ���԰����������
	typedef std::map<const google::protobuf::Descriptor*, std::shared_ptr<Callback> > CallbackMap;

	CallbackMap callbacks_;
	ProtobufMessageCallback defaultCallback_;
};





//dispatcher_lite



#endif  // __PROTOBUF_CODEC_DISPATCHER_H