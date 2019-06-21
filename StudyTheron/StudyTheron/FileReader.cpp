#include "Theron/Theron.h"
#include <iostream>

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

private:
	Theron::Address mClient;	//请求客户的地址
	const char* mFileName;		//请求文件的名称
	bool mProcessed;			//文件是否被读取过
	unsigned char* mBuffer;		//文件内容的缓冲区
	unsigned int mBufferSize;	//缓冲区的大小
	unsigned int mFileSize;		//文件的字节长度
};

static const int MAX_FILES = 16;
static const int MAX_FILE_SIZE = 16384;

int main(int argc, char *argv[])
{
	if (argc < 2)
	{

		return 0;
	}

	// 创建一个worker去处理
	Theron::Framework framework;
	Worker<ReadRequest> worker(framework);

	// 注册一个receiver来捕（catcher）获返回的结果
	Theron::Receiver receiver;
	Theron::Catcher<ReadRequest> resultCatcher;
	receiver.RegisterHandler(&resultCatcher, &Theron::Catcher<ReadRequest>::Push);
	// 命令行上每个文件名称作为请求消息
	for (int i = 0; i < MAX_FILES && i + 1 < argc; ++i)
	{
		unsigned char *const buffer = new unsigned char[MAX_FILE_SIZE];
		const ReadRequest message(
			receiver.GetAddress(),
			argv[i + 1],
			buffer,
			MAX_FILE_SIZE);
		framework.Send(message, receiver.GetAddress(), worker.GetAddress());
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