#include "Theron/Theron.h"
#include <iostream>
#include <vector>
#include <queue>
#pragma comment( lib, "Theron.lib" )
/*
 1.���̰߳��Ⱥ�˳����ִ�в�����
*/
// ͨ�ö��߳�ϵͳ��ʽ��Worker��
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
		// ��Ϣ������const������������Ҫ���������ı��������� 
		WorkMessage result(message);
		result.Process();
		// �����߳���Ϣ��������
		Send(result, from);
	}
};

// ӵ��Process()�ķ���������ΪWorkMessage��һ��Worker����
// ���ݶ�ȡ����: ��ȡһ�������ļ������ݵ�������.
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
		// ���Դ��ļ�
		FILE* handle = fopen(mFileName, "rb");
		if (handle != nullptr)
		{
			// ��ȡ�����ļ�������ʵ�ʶ�ȡ����
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
	Theron::Address mClient;	//����ͻ��ĵ�ַ
	const char* mFileName;		//�����ļ�������
	bool mProcessed;			//�ļ��Ƿ񱻶�ȡ��
	unsigned char* mBuffer;		//�ļ����ݵĻ�����
	unsigned int mBufferSize;	//�������Ĵ�С
	unsigned int mFileSize;		//�ļ����ֽڳ���
};

/*
 2.���̲߳�����ִ�в�����
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
		// �����
		if (message.mProcessed)
		{
			// ������Ϣ
			Send(message, message.mClient);
			// ������ŵ����ж���
			mFreeQueue.push(from);
		}
		else
		{	// ���빤������
			mWorkQueue.push(message);
		}

		// ��������Ϣ����
		while (!mWorkQueue.empty() && !mFreeQueue.empty())
		{
			Send(mWorkQueue.front(), mFreeQueue.front());
			mWorkQueue.pop();
			mFreeQueue.pop();
		}
	}

	std::vector<WorkType*> mWorkers;	//ӵ��Workers��ָ��
	std::queue<Theron::Address> mFreeQueue;	//����workers����
	std::queue<WorkMessage> mWorkQueue;	//δ����Ĺ�����Ϣ����
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

	/* ����1��
	// ����һ��workerȥ����
	Theron::Framework framework;
	Worker<ReadRequest> worker(framework);

	// ע��һ��receiver������catcher���񷵻صĽ��
	Theron::Receiver receiver;
	Theron::Catcher<ReadRequest> resultCatcher;
	receiver.RegisterHandler(&resultCatcher, &Theron::Catcher<ReadRequest>::Push);
	*/

	/* ����2��
	*/
	Theron::Framework::Parameters parameters;
	parameters.mThreadCount = MAX_FILES;
	parameters.mProcessorMask = (1UL << 0) | (1UL << 1);//���ƹ����߳�Ϊǰ������������
	// �������ò�������
	Theron::Framework framework(parameters);
	Theron::Receiver receiver;
	Theron::Catcher<ReadRequest> resultCatcher;
	receiver.RegisterHandler(&resultCatcher, &Theron::Catcher<ReadRequest>::Push);

	Dispatcher<ReadRequest> dispatcher(framework, MAX_FILES);

	/* ���ò��֣�
	*/
	// ���͹�������,��������ÿ���ļ�������Ϊ������Ϣ
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

	// �ȴ����н�� 
	for (int i = 1; i < argc; ++i)
	{
		receiver.Wait();
	}

	// �����������ǽ���ӡ�ļ�������
	ReadRequest result;
	Theron::Address from;
	while (!resultCatcher.Empty())
	{
		resultCatcher.Pop(result, from);
		printf("Read %d bytes from file %s\ntext: %s", result.FileSize(), result.FileName(), result.Buffer());
		// �ͷ�����Ŀռ� 
		delete[] result.Buffer();
	}

	return 0;
}