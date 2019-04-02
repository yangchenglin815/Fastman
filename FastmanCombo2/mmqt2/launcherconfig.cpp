#include "launcherconfig.h"
#include "zlog.h"
#include "zdatabaseutil.h"
#include <QFile>
#include <QStringList>
#include <QNetworkReply>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QEventLoop>
#include "QJson/parser.h"
#include <QtScript/QtScript>
#include "zdmhttp.h"
#include "zstringutil.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>

static QString fromGBK(const QByteArray& data) {
    QTextCodec *c = QTextCodec::codecForName("GB2312");
    if(c == NULL) {
        return QString();
    }
    return c->toUnicode(data);
}

LauncherConfig::LauncherConfig(QObject *parent)
    :QObject(parent)
{

}

bool LauncherConfig::init(const char* url)
{
    QString sAction = "getdesktop";
	QString sBrand = Brand;
	QString sModel = Model;
    QString sUsrID = User;
	QString sTc = Tc;
	QString sIspublic = Ispublic;
    QString sSys = sys;
    QString id = sModel;
    QByteArray respData;

    if(offLine){
    // 得到数据
    // check file exists
    QStringList filePaths;
    filePaths.append(qApp->applicationDirPath() + "/DataFromServer/Desktop/" + (QStringList() << sBrand << sModel << sTc << sIspublic).join("_"));
    filePaths.append(qApp->applicationDirPath() + "/DataFromServer/Desktop/" + (QStringList() << sBrand << "default" << sTc << sIspublic).join("_"));
    filePaths.append(qApp->applicationDirPath() + "/DataFromServer/Desktop/" + (QStringList() << "default" << "default" << sTc << sIspublic).join("_"));
    QString filePath;
    foreach (const QString &path, filePaths) {
        if (QFile::exists(path)) {
            filePath = path;
            break;
        } else {
            DBG("local desktop config file not exsist! filePath=%s\n", qPrintable(path));
        }
    }

    if (filePath.isEmpty()) {
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        DBG("local desktop config file not exsist! fileName=%s\n", qPrintable(file.fileName()));
        return false;
    }

        respData = file.readAll();
    }else{
        ZDMHttp::url_escape(Brand.toUtf8(),sBrand);
        ZDMHttp::url_escape(Model.toUtf8(),sModel);
        ZDMHttp::url_escape(User.toUtf8(),sUsrID);
        DBG("sModel= %s,sBrand= %s\n",sModel.toUtf8().data(),sBrand.toUtf8().data());
        DBG("ispublic= %s,sTc =%s\n",sIspublic.toUtf8().data(),sTc.toUtf8().data());
        if(sTc ==""){
                sTc = "228";
                Tc = "228";
            }
            if(sSys == ""){
                sSys = "win";
            }
            if(sIspublic==""){
               sIspublic="1";
            }
        QString query = QString("action=%1&brand=%2&model=%3&customerid=%4&tcid=%5&sys=%6&ispublic=%7")
                        .arg(sAction, sBrand, sModel, sUsrID, sTc, sSys, sIspublic);
        DBG("%s\n",query.toUtf8().data());
        QUrl sUrl = QUrl(url);
        sUrl.setEncodedQuery(query.toLocal8Bit());
        QNetworkReply* reply = NULL;
        QNetworkAccessManager* manager = new QNetworkAccessManager();
        QNetworkRequest request;
        request.setUrl(sUrl);
        reply = manager->get(request);
        QEventLoop eventLoop;
        connect(manager, SIGNAL(finished(QNetworkReply*)), &eventLoop, SLOT(quit()));
        eventLoop.exec(); //block until finish
        respData = reply->readAll();
    }
    DBG("respData:%s\n",respData.data());
    QString data = respData;
	
    QString s_trashcan;
	QJson::Parser parser;
    QVariantMap map;
    if(!offLine){
        respData = fromGBK(respData).toUtf8();
    }
    map = parser.parse(respData).toMap();
    map = map.value("Desktop").toMap();
	QVariantList list = map.value("position").toList();
    foreach (const QVariant &va,list){
		QVariantMap map = va.toMap();
		PosNode node;
		node.screen = map.value("s").toInt();
        node.x = map.value("x").toInt();
        node.y = map.value("y").toInt();
        node.xn = map.value("xn").toInt();
        node.yn = map.value("yn").toInt();
        node.autoselect = map.value("auto").toBool();
        node.type = map.value("type").toString();
        node.name = map.value("name").toString();
		node.pname = map.value("pname").toString();
		node.cname = map.value("cname").toString();
		node.containID = map.value("container").toInt();
        node.istrashcan = map.value("istrashcan").toInt();//1是套餐应用 2是系统应用
		if(!node.autoselect){
            QStringList list = node.name.split(",");
            if(list.size() == 2){
                node.pname = list.at(0);
                node.cname = list.at(1);
            }
        }
        if(node.type == "A"){
            QStringList list = node.name.split(",");
            if(list.size() == 2){
                node.pname = list.at(0);
                node.cname = list.at(1);
            }
        }
        if(node.type == "F"){
            _folderOrder.append(node);
            if (node.istrashcan > 0){
				s_trashcan = node.name;
			}
        }
        else if(node.type == "W"){
            _widgetOrder.append(node);
        }
        //TYPE_A
        else {
            if(node.autoselect)
                _posOrder.append(node);
            else
                _fixedPkg.append(node);
        }     
		
	}     
	 list = map.value("apks").toList();
	 foreach (const QVariant &va,list){ //解析数组
        QVariantMap map = va.toMap();
        {            
			PkgNode node;
            node.pkgname = map.value("name").toString();
			node.classname = map.value("cname").toString();
            node.inside = map.value("inside").toBool();
            node.foldername = map.value("folder").toString();			
			this->_pkgOrder.append(node);

			if((node.foldername == s_trashcan)&&(node.classname!="")){
				_trashcanapps.insert(node.pkgname,node.classname);
			}
        }
    }
    
    list = map.value("notuninstalls").toList();
	foreach (const QVariant &va,list){ //解析数组
		if(sUninstallList=="")
		{
           QString a0 = va.toString();
           if(a0.contains(".")){
               sUninstallList = va.toString();
           }
		}else{ 
            QString a1 = va.toString();
            if(a1.contains(".")){ //如果是中文的应用，一般是系统自带应用，不需要我们去设置为不可删除名单
                sUninstallList = sUninstallList + ","+ a1;
            }
		}        
	}


    list = map.value("replaceapks").toList();
	foreach (const QVariant &va,list){ //解析数组
        QVariantMap map = va.toMap();
        {
           ReplaceNode rnode;
           rnode.classname = map.value("toapkpackname").toString();
           rnode.lName = map.value("fromapkname").toString();
           for(int it = 0;it != _pkgOrder.size();++it){
               const PkgNode& apk = _pkgOrder.at(it);
               if(apk.classname == rnode.classname){
                   rnode.pkgname = apk.pkgname;
                   DBG("pkgname:%s\n",rnode.pkgname.toUtf8().data());
               }
           }
           this->_replaceOrder.append(rnode);
        }

    }
    //sUninstallList
    map = parser.parse(respData).toMap();
    QString standard = map.value("standard").toString();
    if(standard == "5X5") {
        standardFlag = true;
    }
    QString sCode = map.value("code").toString();
    QString sMsg = map.value("msg").toString();
    DBG("code = %s,msg = %s\n",sCode.toUtf8().data(),sMsg.toUtf8().data());
    if(sCode == "1000"){
        flag = true;
        savedesktopini(id,data,sTc);
    }
    else{
        DBG("sCode is not 1000");
        flag = false;
    }

    return true;
}

void LauncherConfig::readini(QString model, QString TC)
{
    QByteArray respData;
    QSqlQuery query;
    DBG("readini model:%s  TC:%s\n",model.toUtf8().data(),TC.toUtf8().data());
    query.prepare("select data from desktopcfg where id=? and TC=?");
    query.bindValue(0,model);
    query.bindValue(1,TC);
    bool ret = query.exec();
    if(!ret){
        DBG("readini: %s\n",query.lastError().text().toLocal8Bit().data());
    }
    if(query.next()){
        respData = query.value(0).toByteArray();
        DBG("readini:respData:%s\n",respData.data());
        respData = fromGBK(respData).toUtf8();//这样处理过后解析的内容才有
    }

    if(respData==""){
        QString path;
        if(TC == "228"){
            path = QString(qApp->applicationDirPath()+"/defaultdesktopini/%1.txt").arg(TC);
        }else if(TC == "206"){
            path = QString(qApp->applicationDirPath()+"/defaultdesktopini/%1.txt").arg(TC);
        }
        QFile f(path);
        if(!f.open(QIODevice::ReadOnly)){
            QString str = f.errorString();
            DBG("open defaultdesktopini %s failed!%s!\n",TC.toUtf8().data(),f.errorString().toUtf8().data());
            return;
        }
        respData = f.readAll();
        f.close();
        DBG("read file:respData:%s\n",respData.data());
    }
    QString s_trashcan;
    QVariantMap map;
    QJson::Parser parser;
   // respData = fromGBK(respData).toUtf8();//这样处理过后解析的内容才有
    map = parser.parse(respData).toMap();
    map = map.value("Desktop").toMap();
    QVariantList list = map.value("position").toList();
    foreach (const QVariant &va,list){
        QVariantMap map = va.toMap();
        PosNode node;
        node.screen = map.value("s").toInt();
        node.x = map.value("x").toInt();
        node.y = map.value("y").toInt();
        node.xn = map.value("xn").toInt();
        node.yn = map.value("yn").toInt();
        node.autoselect = map.value("auto").toBool();
        node.type = map.value("type").toString();
        node.name = map.value("name").toString();
        node.pname = map.value("pname").toString();
        node.cname = map.value("cname").toString();
        node.containID = map.value("container").toInt();
        node.istrashcan = map.value("istrashcan").toInt();//1是套餐应用 2是系统应用
        if(!node.autoselect){
            QStringList list = node.name.split(",");
            if(list.size() == 2){
                node.pname = list.at(0);
                node.cname = list.at(1);
            }
        }
        if(node.type == "A"){
            QStringList list = node.name.split(",");
            if(list.size() == 2){
                node.pname = list.at(0);
                node.cname = list.at(1);
            }
        }
        if(node.type == "F"){
            _folderOrder.append(node);
            if (node.istrashcan > 0){
                s_trashcan = node.name;
            }
        }
        else if(node.type == "W"){
            _widgetOrder.append(node);
        }
        //TYPE_A
        else {
            if(node.autoselect)
                _posOrder.append(node);
            else
                _fixedPkg.append(node);
        }

    }
     list = map.value("apks").toList();
     foreach (const QVariant &va,list){ //解析数组
        QVariantMap map = va.toMap();
        {
            PkgNode node;
            node.pkgname = map.value("name").toString();
            node.classname = map.value("cname").toString();
            node.inside = map.value("inside").toBool();
            node.foldername = map.value("folder").toString();
            this->_pkgOrder.append(node);

            if((node.foldername == s_trashcan)&&(node.classname!="")){
                _trashcanapps.insert(node.pkgname,node.classname);
            }
        }
    }

    list = map.value("notuninstalls").toList();
    foreach (const QVariant &va,list){ //解析数组
        if(sUninstallList=="")
        {
          QString a0 = va.toString();
          if(a0.contains(".")){
           sUninstallList = va.toString();
          }
        }else{
        QString a1 = va.toString();
        if(a1.contains(".")){ //如果是中文的应用，一般是系统自带应用，不需要我们去设置为不可删除名单
          sUninstallList = sUninstallList + ","+ a1;

        }
        }
    }


    list = map.value("replaceapks").toList();
    foreach (const QVariant &va,list){ //解析数组
        QVariantMap map = va.toMap();
        {
/*
            ////////////////////////////////////////////
            PkgNode node;
            node.pkgname = map.value("name").toString();
            node.classname = map.value("cname").toString();
            node.inside = map.value("inside").toBool();
            node.foldername = map.value("folder").toString();
            this->_pkgOrder.append(node);

            if((node.foldername == s_trashcan)&&(node.classname!="")){
                _trashcanapps.insert(node.pkgname,node.classname);
            }
            ////////////////////////////
            */
           ReplaceNode rnode;
           rnode.classname = map.value("toapkpackname").toString();
           rnode.lName = map.value("fromapkname").toString();
           for(int it = 0;it != _pkgOrder.size();++it){
               const PkgNode& apk = _pkgOrder.at(it);
               if(apk.classname == rnode.classname){
                   rnode.pkgname = apk.pkgname;
                   DBG("pkgname:%s\n",rnode.pkgname.toUtf8().data());
               }
           }
           this->_replaceOrder.append(rnode);
        }

    }
    //sUninstallList
    map = parser.parse(respData).toMap();
    QString sCode = map.value("code").toString();
    QString sMsg = map.value("msg").toString();
    DBG("readini:code = %s,msg = %s\n",sCode.toUtf8().data(),sMsg.toUtf8().data());
}

bool LauncherConfig::contains(const char *pkgname)
{
    bool matched = false;
    foreach(const PkgNode& n,_pkgOrder){
        if(n.pkgname == pkgname){
            matched = true;
            break;
        }
    }
    return matched;
}

void LauncherConfig::savedesktopini(QString id, QString data, QString TC)
{
    DBG("save desktopcfg config");
    QSqlQuery query;
    int flag;
    QString sql = "create table if not exists desktopcfg(id TEXT,data TEXT,TC TEXT)";
    if (!query.exec(sql)) {
        DBG("savedesktopcfg: %s\n", query.lastError().text().toLocal8Bit().data());
    }

    query.prepare("select count(*) from desktopcfg where id=? and TC=?");
    query.bindValue(0,id);
    query.bindValue(1,TC);
    bool ret = query.exec();
    if (!ret) {
        DBG("savedesktopcfg: %s\n", query.lastError().text().toLocal8Bit().data());
    }
    if(query.next()){
        flag = query.value(0).toInt();
    }
    if(flag == 0){
    query.prepare("INSERT INTO desktopcfg (id,data,TC) VALUES(?,?,?)");
    query.bindValue(0, id);
    query.bindValue(1, data);
    query.bindValue(2,TC);
    ret = query.exec();
    if (!ret) {
        DBG("savedesktopcfg: %s\n", query.lastError().text().toLocal8Bit().data());
    }
    }
    else{
        query.prepare("update desktopcfg set data=? where id=? and TC=?");
        query.bindValue(0,data);
        query.bindValue(1,id);
        query.bindValue(2,TC);
        ret = query.exec();
        if (!ret) {
            DBG("savedesktopcfg: %s\n", query.lastError().text().toLocal8Bit().data());
        }
    }
}

bool LauncherConfig::inside(const char *pkgname, QString &folder)
{
    bool matched = false;
    foreach(const PkgNode& n,_pkgOrder){
        if(n.pkgname == pkgname){
            if(n.inside){
                matched = true;
                folder = n.foldername;
            }
            break;
        }
    }
    return matched;
}


const PosNode &LauncherConfig::at(int it)
{
    return _posOrder.at(it);
}

LauncherConfig::~LauncherConfig()
{

}

int LauncherConfig::size()
{
	return _posOrder.size();
}
