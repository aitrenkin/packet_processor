#include <iostream>
#include <vector>
/************************************************************************/
struct IReceiver
{
    virtual ~IReceiver() = default;
    virtual void Receive(const char* data, std::size_t size) = 0;
};

struct ICallback
{
    virtual ~ICallback() = default;
    virtual void BinaryPacket(const char* data, std::size_t size) = 0;
    virtual void TextPacket(const char* data, std::size_t size) = 0;
};
/************************************************************************/

class PacketProcessor : private IReceiver, private ICallback
{
public:
    enum class ProcessorState {Unknown, GettingTextPacket, GettingBinaryPacket, Waiting};
    PacketProcessor() { m_state = ProcessorState::Waiting; }
    virtual void Receive(const char* data, std::size_t size) override
    {
        if(m_state == ProcessorState::Waiting)
        {
            if(size < 1)
                return; // warn
            m_buffer.clear();
            if(data[0] == 0x24)
            {
                m_state = ProcessorState::GettingBinaryPacket;
                getBinaryPacket(data, size);
            }
            else
            {
                m_state = ProcessorState::GettingTextPacket;
                getTextPacket(data,size);
            }
        }
        else if(m_state == ProcessorState::GettingBinaryPacket)
            getBinaryPacket(data, size);
        else if(m_state == ProcessorState::GettingTextPacket)
            getTextPacket(data, size);
    }
    virtual void BinaryPacket(const char* data, std::size_t size) override
    {
        std::cout  << "Got binary packet size: "  << size << std::endl;
    }
    virtual void TextPacket(const char* data, std::size_t size) override
    {
        std::cout  << "Got text packet size: "  << size << std::endl;
        std::cout << "Text: " << std::string(data,size) << std::endl;
    }
private:
    void appendToInternalBuffer(const char *data, size_t size)
    {
        for(size_t i = 0; i < size; i++)
            m_buffer.push_back(data[i]);
    }
    void flushTextPacket(int textSize)
    {
        char *buf = new char[textSize];
        for(int i = 0; i < textSize ; i++)
            buf[i] = m_buffer[i];
        TextPacket(buf, textSize);
    }
    int textEndPosition()
    {
        for(size_t i = 0; i < m_buffer.size(); i++)
        {
            if(m_buffer[i] == '\r'
                    && i + 3 < m_buffer.size()
                    && m_buffer[i+1] == '\n' && m_buffer[i+2] == '\r' && m_buffer[i+3] == '\n')
                return i;
        }
        return -1;
    }
    void flushBinaryPacket()
    {
        char *buf = new char[m_binarySize];
        for(size_t i = 5; i < m_binarySize + 5; i++)
            buf[i-5] = m_buffer[i];
        BinaryPacket(buf, m_binarySize);
    }

    unsigned int getBinarySizeFromRaw(char raw[])
    {
        unsigned int result;
        char *index = (char*)&result;
        size_t size_int = sizeof(unsigned int);
        for(size_t i = 0; i < size_int; i++)
            index[i] = raw[i]; // todo arch test
        return result;
    }
    void getBinaryPacket(const char *data, size_t size)
    {
            //только начали читать бинарный пакет
            appendToInternalBuffer(data, size);
            if(m_buffer.size() < 5) // 1 байт заголовок + 4 байта размер bin - пакета
                return;

            if(m_binarySize == 0)
            {
                char s[4];
                for(size_t i = 1; i < 5; i++)
                    s[i-1] = m_buffer[i];
                m_binarySize = getBinarySizeFromRaw(s);
            }

            if(m_buffer.size() - 5 >= m_binarySize) //набрали весь пакет или даже больше
            {
                flushBinaryPacket();
                m_buffer.erase(m_buffer.begin(), m_buffer.begin() + m_binarySize + 5);
                m_binarySize = 0;
                if(m_buffer.size() == 0)
                    m_state = ProcessorState::Waiting; // буфер пуст, ждем новый пакет
                else
                {
                    if(m_buffer[0] != 0x24)
                    {
                        m_state = ProcessorState::GettingTextPacket;
                        getTextPacket(data,0);
                    }
                    else
                        getBinaryPacket(data, 0);
                }
            }

    }
    void getTextPacket(const char *data, size_t size)
    {
        appendToInternalBuffer(data, size);
        int t_end = textEndPosition();
        if(t_end != -1)
        {
            flushTextPacket(t_end);
            m_buffer.erase(m_buffer.begin(), m_buffer.begin() + t_end + 4);
            if(m_buffer.size() == 0)
                m_state = ProcessorState::Waiting; // буфер пуст, ждем новый пакет
            else
            {
                if(m_buffer[0] == 0x24)
                {
                    m_state = ProcessorState::GettingBinaryPacket;
                    getBinaryPacket(data,0);
                }
                else
                    getTextPacket(data, 0);
            }
        }
    };
    ProcessorState m_state;
    std::vector<char> m_buffer;
    unsigned int  m_binarySize = 0;
};

int main()
{
    PacketProcessor pp;
    char buf[] = "hello\r\n\r\nworld2\r\n\r\n$\5\0\0\0binarfoobarbaz\r\n\r\n";
    pp.Receive(buf,sizeof(buf));
    //char bufBinaryOnly[] = "$\5\0\0\0hello$\7\0\0\08181818";
    //pp.Receive(bufBinaryOnly, sizeof(bufBinaryOnly));
    return 0;
}
