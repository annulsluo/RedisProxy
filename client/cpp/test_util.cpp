/*************************************************************************
    > File Name: test_promutheus.cpp
    > Author: annulsluo
    > Mail: annulsluo@webank.com
    > Created Time: 日  3/ 1 19:57:25 2020
 ************************************************************************/
#include <map>
#include <memory>
#include <string>
#include <thread>

struct FamilyItem
{
    std::string sFamilyItem;
    std::string sHelp;
	std::string sLable;
};

static FamilyItem Familys[] =
{
    // add Famliy (mertric)
    {"RedisProxyMonitor", "RedisProxyMonitor Status", "test1"},
    {"RedisProxyMonitor", "RedisProxyMonitor Status", "appid"},
    {"RedisProxyMonitor", "RedisProxyMonitor Status", ""},
    {"RedisProxyMonitor", "RedisProxyMonitor Status", ""},
};
class base
	{
		public :
			base(){};
			~base(){};
		int basea;
	};

class Base
{
	public: 
		public:
	int Basea;
	Base(){};
	~Base(){};
};

class A: public Base
{
	public: 
	class base
	{
		public :
	base(){};
	~base(){};
		int baseA;
	};
	public:
	int Aa;
	A(){};
	~A(){};
};

class B: public Base
{
	public: 
		class base
		{
		public :
			base(){};
			~base(){};
			int baseB;
		};
	public:
	int Ba;
	B(){};
	~B(){};

};

int main(int argc, char** argv) {
	Base::base * pbasea = new A::base();
	Base::base * pbaseb = new B::base();
 for( size_t i = 0; i < sizeof( Familys ) / sizeof( FamilyItem ); ++ i )
    {
		printf( "index:%d, family:%s help:%s label:%s\n",i, Familys[i].sFamilyItem.c_str(), Familys[i].sHelp.c_str(), Familys[i].sLable.c_str() );
    }
   return 0;
}
