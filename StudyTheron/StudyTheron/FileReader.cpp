#include "Theron/Theron.h"
#include <iostream>
#include <vector>
#include <queue>
#pragma comment( lib, "Theron.lib" )
/*
 1.单线程按先后顺序来执行操作的
*/
// 通用多线程系统形式“Worker”
template<typename WorkMessage>
class Worker : public Theron::Actor
{
public:
	Worker(Theron::Framework& framework) : Theron::Actor(framework)
	{
		RegisterHandler(this, &Worker::Handler);
	}

private:
	void Handler(const WorkMessage &message, const Theron::Address from)
	{
		// 消息参数是const，所以我们需要拷贝它来改变它的性质 
		WorkMessage result(message);
		result.Process();
		// 返回线程消息给发送者
		Send(result, from);
	}
};

// 拥有Process()的方法可以作为WorkMessage被一个Worker处理
// 数据读取请求: 读取一个磁盘文件的内容到缓存区.
class ReadRequest
{
public:
	explicit ReadRequest(const Theron::Address client = Theron::Address(),
		const char *const fileName = 0,
		unsigned char *const buffer = 0,
		const unsigned int bufferSize = 0) : 
		mClient(client),
		mFileName(fileName),
		mProcessed(false),
		mBuffer(buffer),
		mBufferSize(bufferSize),
		mFileSize(0)
	{

	}

	void Process()
	{
		mProcessed = true;
		mFileSize = 0;
		// 尝试打开文件
		FILE* handle = fopen(mFileName, "rb");
		if (handle != nullptr)
		{
			// 读取数据文件，设置实际读取长度
			mFileSize = (uint32_t)fread(mBuffer, sizeof(unsigned char), mBufferSize, handle);
			mBuffer[mFileSize] = 0;
			fclose(handle);
		}
	}

	int FileSize()
	{
		return mFileSize;
	}

	const char* FileName()
	{
		return mFileName;
	}

	unsigned char* Buffer()
	{
		return mBuffer;
	}

	bool Processed()
	{
		return mProcessed;
	}

	Theron::Address Client()
	{
		return mClient;
	}

public:
	Theron::Address mClient;	//请求客户的地址
	const char* mFileName;		//请求文件的名称
	bool mProcessed;			//文件是否被读取过
	unsigned char* mBuffer;		//文件内容的缓冲区
	unsigned int mBufferSize;	//缓冲区的大小
	unsigned int mFileSize;		//文件的字节长度
};

/*
 2.多线程并行来执行操作的
*/
template <typename WorkMessage>
class Dispatcher : public Theron::Actor
{
public:
	Dispatcher(Theron::Framework &framework, const int WorkerCount) : Theron::Actor(framework)
	{
		for (int i = 0; i < WorkerCount; ++i)
		{
			mWorkers.push_back(new WorkType(framework));
			mFreeQueue.push(mWorkers.back()->GetAddress());
		}

		RegisterHandler(this, &Dispatcher::Handler);
	}

	~Dispatcher()
	{
		const int nSize(static_cast<int>(mWorkers.size()));
		for (int i = 0; i < nSize; ++i)
		{
			delete mWorkers[i];
		}
	}

private:
	typedef Worker<WorkMessage> WorkType;

	void Handler(const WorkMessage &message, const Theron::Address from)
	{
		// 处理过
		if (message.mProcessed)
		{
			// 发送消息
			Send(message, message.mClient);
			// 处理完放到空闲队列
			mFreeQueue.push(from);
		}
		else
		{	// 加入工作队列
			mWorkQueue.push(message);
		}

		// 处理工作消息队列
		while (!mWorkQueue.empty() && !mFreeQueue.empty())
		{
			Send(mWorkQueue.front(), mFreeQueue.front());
			mWorkQueue.pop();
			mFreeQueue.pop();
		}
	}

	std::vector<WorkType*> mWorkers;	//拥有Workers的指针
	std::queue<Theron::Address> mFreeQueue;	//空闲workers队列
	std::queue<WorkMessage> mWorkQueue;	//未处理的工作消息队列
};

static const int MAX_FILES = 16;
static const int MAX_FILE_SIZE = 16384;

int main(int argc, char *argv[])
{
	if (argc < 2)
	{
		printf("Expected up to 16 file name arguments.\n");
		return 0;
	}

	/* 方案1：
	// 创建一个worker去处理
	Theron::Framework framework;
	Worker<ReadRequest> worker(framework);

	// 注册一个receiver来捕（catcher）获返回的结果
	Theron::Receiver receiver;
	Theron::Catcher<ReadRequest> resultCatcher;
	receiver.RegisterHandler(&resultCatcher, &Theron::Catcher<ReadRequest>::Push);
	*/

	/* 方案2：
	*/
	Theron::Framework::Parameters parameters;
	parameters.mThreadCount = MAX_FILES;
	parameters.mProcessorMask = (1UL << 0) | (1UL << 1);//限制工作线程为前两个处理器核
	// 根据配置参数创建
	Theron::Framework framework(parameters);
	Theron::Receiver receiver;
	Theron::Catcher<ReadRequest> resultCatcher;
	receiver.RegisterHandler(&resultCatcher, &Theron::Catcher<ReadRequest>::Push);

	Dispatcher<ReadRequest> dispatcher(framework, MAX_FILES);

	/* 共用部分：
	*/
	// 发送工作请求,命令行上每个文件名称作为请求消息
	for (int i = 0; i < MAX_FILES && i + 1 < argc; ++i)
	{
		unsigned char* const buffer = new unsigned char[MAX_FILE_SIZE];
		const ReadRequest message(
			receiver.GetAddress(),
			argv[i + 1],
			buffer,
			MAX_FILE_SIZE
		);

		framework.Send(message, receiver.GetAddress(), dispatcher.GetAddress());
	}

	// 等待所有结果 
	for (int i = 1; i < argc; ++i)
	{
		receiver.Wait();
	}

	// 处理结果，我们仅打印文件的名称
	ReadRequest result;
	Theron::Address from;
	while (!resultCatcher.Empty())
	{
		resultCatcher.Pop(result, from);
		printf("Read %d bytes from file %s\ntext: %s", result.FileSize(), result.FileName(), result.Buffer());
		// 释放申请的空间 
		delete[] result.Buffer();
	}

	return 0;
}