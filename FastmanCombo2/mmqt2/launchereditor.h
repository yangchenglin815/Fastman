#ifndef LAUNCHEREDITOR_H
#define LAUNCHEREDITOR_H

#include <iostream>
#include <vector>
#include <fastman2.h>
#include <QQueue>
#include <QtCore/qstringlist.h>

#if !defined(_WIN32)
 #define BUFSIZ  8192
#endif

struct FPkg{
        QString pkgname;       
        QString classname;
		bool istrashcan;
};

struct BPkg{  //底部四大天王应用
        QString pkgname;       
        QString classname;
		int x;
		int y;
};


struct RPkg{
    QString fromclassname;
    QString frompkgname;
    QStringList lname;
    QString toclassname;
    QString topkgname;
};

class LauncherConfig;
class LauncherEditor
{
    LauncherConfig* _config;
    static void cp(char* dest,int size,const char* src);
	bool _screen01;
	bool _screen02;
	bool _screen03;
	bool _autofolder;

public:
    struct Pkg{
        int s,x,y;
        char pname[BUFSIZ*2];
        char ccname[BUFSIZ*4];
        Pkg(int,int,int,const char*,const char*);
    };
	struct UPkg{
		int s,x,y;
		char rpath[BUFSIZ*4];
		UPkg(int,int,int,const char*);
	};
    struct rPkg{
        char frompname[BUFSIZ*2];
        char fromcname[BUFSIZ*4];
        char topname[BUFSIZ*2];
        char tocname[BUFSIZ*4];
        rPkg(const char*,const char*,const char*,const char*);
    };

    struct Folder{
        int s,x,y,containID;
        int istrashcan;
        char fname[BUFSIZ];
		QString sName;
        std::vector<Pkg*> _pkgs;
		std::vector<UPkg*> _upkgs;		
		QQueue<FPkg*> _FPkg;		
        Folder(int s,int x,int y,const char* fname,int containID,int istrashcan, QString sName);
		//InsertApk();
        bool operator == (const Folder& f);
    };
    struct Widget{
        int s,x,y,xn,yn;
        char rname[BUFSIZ*4];
        char pname[BUFSIZ*2];
        Widget(int s, int x, int y, int xn, int yn, const char *pname, const char* rname);
    };
    LauncherEditor();
    ~LauncherEditor();
    Pkg* addPkg(int s, int x, int y, const char* pname, const char* cname, Folder* fname = NULL);
	UPkg* addUPkg(int s, int x,int y,const char* rpath,Folder* fname = NULL);
	void clearPkg();
    Folder* addFolder(int s,int x,int y,const char* fname, int containID, int istrashcan, QString sName);
    Folder* search(const char* fname);
    Widget* addWidget(int s, int x, int y, int xn, int yn, const char *pname, const char* rname);
    bool commit(const char* spath);
    bool installsucceed(QString pkg);
    bool recommit(const char* spath);
    bool desktopCommit(const char* spath);
    bool hiddenCommit(const char* spath);
	inline bool validate(){return _screen01 && _screen02 && _screen03;}
	inline void autoFolder(bool auto_ = true){_autofolder = auto_;}

	QList<AGENT_FILTER_INFO*> m_filter;
public:
	QString sUninstallList;
    QString Serial;
    bool useAsync;
    bool standard;
	QQueue<BPkg*> _bottomOrder;
    QQueue<RPkg*> _ReplaceOrder;
	QQueue<FPkg*> _AllFoldPkg;	
    std::vector<Folder*> folders_;
    QStringList listTC;
    QStringList ListPkg;
    bool installGameHouse;
	QString Brand;
    QString hiddenShowTime;
    QStringList hiddenPkg;
    QStringList appmarket;

private:
    std::vector<Pkg*> pkgs_;
    std::vector<rPkg*> rpkg_;
	std::vector<UPkg*> upkgs_;
    std::vector<Widget*> widgets_;
};

#endif // LAUNCHEREDITOR_H
