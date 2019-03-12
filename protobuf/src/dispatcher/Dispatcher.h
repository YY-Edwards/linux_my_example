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

	//示例:
	//C++中基类采用virtual虚析构函数是为了防止内存泄漏。
	//具体地说，如果派生类中申请了内存空间，并在其析构函数中对这些内存空间进行释放。
	//假设基类中采用的是非虚析构函数，当删除基类指针指向的派生类对象时就不会触发动态绑定，因而只会调用基类的析构函数，
	//而不会调用派生类的析构函数。那么在这种情况下，派生类中申请的空间就得不到释放从而产生内存泄漏。
	//所以，为了防止这种情况的发生，C++中基类的析构函数应采用virtual虚析构函数。
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
		//另外如果在积累中含有一个或多个虚函数，我们可以使用dynamic_cast请求一个类型转换，该转换的安全检查将在运行时执行。
		//如果已知某个基类想派生类的转换是安全的，则可以直接使用static_cast来强制覆盖，如下面的操作，只是这个智能指针的类型转换
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

	//定义一个模板成员函数：可以接受注册任意消息类型的T的回调，
	//然后它创建一个模板化的派生类CallbackT<T>,这样消息的类型信息就保存在CallbackT<T>中
	//registerMessageCallback()里的参数是使用的模板类里的成员变量，即相当于需要实现模板类外实现声明一个变量，那么需要用typename声明CallbackT<T>是一个类
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

			//回避虚函数机制，使用作用域符号强行调用基类的版本,用如下操作
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
	//为什么是基类指针：这样既可以绑定基类对象也可以绑定派生类对象
	typedef std::map<const google::protobuf::Descriptor*, std::shared_ptr<Callback> > CallbackMap;

	CallbackMap callbacks_;
	ProtobufMessageCallback defaultCallback_;
};





//dispatcher_lite



#endif  // __PROTOBUF_CODEC_DISPATCHER_H