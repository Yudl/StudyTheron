#include <iostream>
#include "Theron\Theron.h"
#pragma comment( lib, "Theron.lib" )

// 用户定义的 Actor 总需要继承于 Theron::Actor// 每个 Actor 和应用程序的其他部分通信的唯一途径就是通过消息 
class Printer : public Theron::Actor
{
public:
	// 注册消息的处理函数 
	explicit Printer(Theron::Framework &framework) : Theron::Actor(framework)
	{
		RegisterHandler(this, &Printer::print);
	}
private:
	// 消息处理函数的第一个参数指定了处理的消息的类型 
	void print(const std::string &message, const Theron::Address from)
	{
		std::cout << message.c_str() << std::endl;

		// 返回一个虚假（dummy）消息来实现同步 
		if (!Send(0, from))
		{
			std::cout << "Failed Send Message!!!" << std::endl;
		}
	};
};

int main1()
{
	// 构造Framework对象,并且实例化一个Printer的Actor由它管理
	Theron::Framework framework;
	Printer printer(framework);

	// 构造一个Receiver对象用于接收Actor反馈的消息
	// 用于在非 Actor 代码中（例如 main 函数中）与 Actor 通信 
	Theron::Receiver recviver;

	// 发送消息给printer这个Actor
	if (!framework.Send(std::string("Hellow Theron!!!"), recviver.GetAddress(), printer.GetAddress()))
	{
		std::cout << "Send Error!!!" << std::endl;
	}

	// Receiver的Wait()方法，用来等待接收到actor的反馈消息才结束主线程，也就是整个进程
	recviver.Wait();

	return 0;
}