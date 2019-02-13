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
//不能直接创建抽象基类
class Callback : muduo::noncopyable//含有纯虚函数的类是抽象基类，负责定义接口，后续的其他类则可以覆盖该接口
{

public :

	//1.每个析构函数（不加 virtual） 只负责清除自己的成员。
	//2.可能有基类指针指向的确是派生类成员的情况。（这是很正常的）
	//	那么当析构一个指向派生类成员的基类指针时，程序就是未定义（因为此时可能是静态联编）。可能只析构基类，派生类仍然存在。
	//  如果在 派生类中 分配了 内存空间的话，没有将基类的析构函数声明为虚析构函数，很容易发生内存泄漏事件。
	//	所以要保证运行适当的析构函数，基类中的析构函数必须为虚析构。
	virtual ~Callback() = default;//C++11中的默认行为。对析构函数进行动态绑定
	virtual void onMessage(const muduo::net::TcpConnectionPtr&,
							const MessagePtr&,
							muduo::Timestamp)const = 0;//声明为纯虚函数，则不需要定义。

};

//定义一个类模板
template <typename T>
class CallbackT : public Callback
{
	//通过std::is_base_of()确保传入T类型是继承来自google::protobuf::Message
	static_assert(std::is_base_of<google::protobuf::Message, T>::value,
		"T must be derived from gpb::Message.");

public:

	typedef std::function<void (const muduo::net::TcpConnectionPtr&,
								const std::shared_ptr<T>& message,//类型为T的智能指针，并指向message
								muduo::Timestamp)> ProtobufMessageTCallback;



	CallbackT(const ProtobufMessageTCallback& callback)
		:callback_(callback)
	{

	}
	~CallbackT() = default;

	void onMessage(const muduo::net::TcpConnectionPtr& conn,
					const MessagePtr& message,
					muduo::Timestamp receiveTime) const override//c++11 中明确需要重写接口的关键字
	{

		//onMessage -> callback_
		//需要用到基类转派生类的类型转换。
		//基类到派生类不存在隐式转换；
		//派生类到基类的类型转换是因为每一个派生类都包含一个基类部分；
		//基类可能是派生类的一部分，也有可能不是，所以不存在基类到派生类的自动类型转换。
		//专门针对shared_ptr类型：Static cast of shared_ptr
		//share_ptr为了支持转型，所以提供了类似的转型函数即static_pointer_cast<T>，
		//从而使转型后仍然为shared_pointer对象，仍然对指针进行管理；
		std::shared_ptr<T> concrete = std::static_pointer_cast<T>(message);
		//std::shared_ptr<T> concrete = muduo::down_pointer_cast<T>(message);
		assert(concrete != NULL);
		callback_(conn, concrete, receiveTime);

	}

private:

	ProtobufMessageTCallback callback_;
};


class ProtobufDisapatcher
{
public:

	typedef std::function<void(const muduo::net::TcpConnectionPtr&,
								const MessagePtr&,
								muduo::Timestamp)> ProtobufMessageCallback;


	explicit ProtobufDisapatcher(const ProtobufMessageCallback& defaultCb)
		:defaultCallback_(defaultCb)
	{
	}
	~ProtobufDisapatcher();

	//定义一个模板成员函数：可以接受注册任意消息类型的T的回调，
	//然后它创建一个模板化的派生类CallbackT<T>,这样消息的类型信息就保存在CallbackT<T>中
	template<typename T>
	void registerMessageCallback(const typename CallbackT<T>::ProtobufMessageTCallback& callback)
	{
		std::shared_ptr<CallbackT<T>> pd(new CallbackT<T>(callback));//构造一个新的模板类，并传入上层回调
		//实参绑定的是派生类指针
		callbacks_[T::descriptor()] = pd;//根据消息类型将类指针添加到map中
	}

	void onProtobufMessage(const muduo::net::TcpConnectionPtr& conn,
							const MessagePtr& message,
							muduo::Timestamp receiveTime)const
	{
		CallbackMap::const_iterator it = callbacks_.find(message->GetDescriptor());
		if (it != callbacks_.end())
		{
			//这里，根据引用或指针所绑定的类型不同，来决定是调用基类的版本还是派生类版本
			//动态绑定只有在我们通过指针或引用调用虚函数时才会发生

			//回避虚函数机制，强行调用基类的版本
			//it->second->Callback::onMessage(conn, message, receiveTime);
			it->second->onMessage(conn, message, receiveTime);//回调具体的消息类型，并回调用户回调
		}
		else//设置的默认回调（调试用）
		{
			defaultCallback_(conn, message, receiveTime);
		}
	
	}

private:

	//具体的消息类型的回调map
	//生成proto文件时，已缓存了消息格式的所有类型。
	//每个具体的消息类型都一个全局的Descriptor对象，其地址是不变的，可以用来做key
	//value为不同回调的智能指针：类继承的动态绑定
	typedef std::map<const google::protobuf::Descriptor*, std::shared_ptr<Callback> > CallbackMap;

	CallbackMap callbacks_;
	ProtobufMessageCallback defaultCallback_;
};





//dispatcher_lite



#endif  // __PROTOBUF_CODEC_DISPATCHER_H