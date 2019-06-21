#include <iostream>
#include "Theron\Theron.h"
#pragma comment( lib, "Theron.lib" )

// �û������ Actor ����Ҫ�̳��� Theron::Actor// ÿ�� Actor ��Ӧ�ó������������ͨ�ŵ�Ψһ;������ͨ����Ϣ 
class Printer : public Theron::Actor
{
public:
	// ע����Ϣ�Ĵ����� 
	explicit Printer(Theron::Framework &framework) : Theron::Actor(framework)
	{
		RegisterHandler(this, &Printer::print);
	}
private:
	// ��Ϣ�������ĵ�һ������ָ���˴������Ϣ������ 
	void print(const std::string &message, const Theron::Address from)
	{
		std::cout << message.c_str() << std::endl;

		// ����һ����٣�dummy����Ϣ��ʵ��ͬ�� 
		if (!Send(0, from))
		{
			std::cout << "Failed Send Message!!!" << std::endl;
		}
	};
};

int main1()
{
	// ����Framework����,����ʵ����һ��Printer��Actor��������
	Theron::Framework framework;
	Printer printer(framework);

	// ����һ��Receiver�������ڽ���Actor��������Ϣ
	// �����ڷ� Actor �����У����� main �����У��� Actor ͨ�� 
	Theron::Receiver recviver;

	// ������Ϣ��printer���Actor
	if (!framework.Send(std::string("Hellow Theron!!!"), recviver.GetAddress(), printer.GetAddress()))
	{
		std::cout << "Send Error!!!" << std::endl;
	}

	// Receiver��Wait()�����������ȴ����յ�actor�ķ�����Ϣ�Ž������̣߳�Ҳ������������
	recviver.Wait();

	return 0;
}