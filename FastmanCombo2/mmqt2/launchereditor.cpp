#include "launchereditor.h"
#include <QDomDocument>
#include <QFile>
#include <QTextStream>
#include <algorithm>
#include <stdio.h>
#include "zlog.h"
#include <QTextCodec>
#include <QSettings>
#include "quuid.h" 
#include <QApplication>
#include "adbclient.h"

void LauncherEditor::cp(char *dest, int size, const char *src)
{
    memset(dest,0,size);
    int len = strlen(src);
    strncpy(dest,src,len>size?size:len);
}

LauncherEditor::LauncherEditor()
    :_screen01(false),_screen02(false),_screen03(false)
{

}

LauncherEditor::~LauncherEditor()
{

}

LauncherEditor::Pkg *LauncherEditor::addPkg(int s, int x, int y, const char *pname, const char *cname, LauncherEditor::Folder *fname)
{
    Pkg* pkg = new Pkg(s,x,y,pname,cname);
    if(fname != NULL){
        if(fname->s == 1){
            _screen01 = true;
        }
        else if(fname->s == 2){
            _screen02 = true;
        }
        else if(fname->s == 3){
            _screen03 = true;
        }
        // 去重
        bool matched = false;
        if(QString(pkg->pname)!="" && QString(pkg->ccname)!=""){
            for(int it = 0;it != fname->_pkgs.size();++it){
                const Pkg* item = fname->_pkgs.at(it);
                if(0 == strcmp(item->ccname,pkg->ccname) &&
                        0 == strcmp(item->pname,pkg->pname)){
                    matched = true;
                    break;
                }
            }
        }
        DBG("add to folder, s: %d, x: %d, y: %d, package: %s, folder: %s, matched(%s)",
            fname->s, fname->x, fname->y, pname, fname->fname, matched?"true":"false");
        if(!matched){
            fname->_pkgs.push_back(pkg);
        }
    }
    else {
        if(s == 1){
            _screen01 = true;
        }
        else if(s == 2){
            _screen02 = true;
        }
        else if(s == 3){
            _screen03 = true;
        }
        else {

        }
        //去重
        bool matched = false;
        if(QString(pkg->pname)!="" && QString(pkg->ccname)!=""){
            for(int it = 0;it != this->pkgs_.size();++it){
                const Pkg* item = this->pkgs_.at(it);
                if(0 == strcmp(item->ccname,pkg->ccname) &&
                        0 == strcmp(item->pname,pkg->pname)){
                    matched = true;
                    break;
                }
            }
        }
        DBG("add app, s: %d, x: %d, y: %d, package: %s, matched(%s)", s, x, y, pname, matched?"true":"false");
        if(!matched){
            this->pkgs_.push_back(pkg);
        }
    }
    return pkg;
}

LauncherEditor::Folder *LauncherEditor::addFolder(int s, int x, int y, const char *fname, int containID, int istrashcan, QString sName)
{
    Folder* f = new Folder(s,x,y,fname,containID,istrashcan,sName);
    this->folders_.push_back(f);
    return f;
}
bool cmp (LauncherEditor::Folder* f1,const char* name){
    return (0 == strcmp(f1->fname,name));
}

LauncherEditor::Folder *LauncherEditor::search(const char *fname)
{
    for(int it = 0;it != folders_.size();it++){
        Folder* f = folders_.at(it);
        if(cmp(f,fname)){
            return f;
        }
    }
    return NULL;
}

LauncherEditor::Widget *LauncherEditor::addWidget(int s, int x, int y, int xn, int yn, const char* pname,const char *rname)
{
    Widget* w = new Widget(s,x,y,xn,yn,pname,rname);
    this->widgets_.push_back(w);
    return w;
}

static QString fromGBK(const QByteArray& data) {
    QTextCodec *c = QTextCodec::codecForName("GB2312");
    if(c == NULL) {
        return QString();
    }
    return c->toUnicode(data);
}

bool LauncherEditor::hiddenCommit(const char *spath){
    QFile file(spath);
    file.remove();
    if(!file.open(QIODevice::WriteOnly)){
        return false;
    }
    QTextStream out(&file);
    out.setCodec("UTF-8");
    QDomDocument doc;
    QDomProcessingInstruction instruction = doc.createProcessingInstruction("xml", "version=\"1.0\" encoding=\"UTF-8\"");
    doc.appendChild(instruction);
    QDomElement root = doc.createElement("module");
    doc.appendChild(root);
    QDomElement a = doc.createElement("show_folder_apps_time");
    QDomText text = doc.createTextNode(hiddenShowTime);
    a.appendChild(text);
    root.appendChild(a);
    QDomElement b = doc.createElement("hide_folder_apps_packNames");
    QString pkgname;
    for(int it = 0;it < hiddenPkg.size();it++){
        if(it == 0){
            pkgname = hiddenPkg.at(it);
        }else{
            pkgname = pkgname + "," +hiddenPkg.at(it);
        }
    }
    text = doc.createTextNode(pkgname);
    b.appendChild(text);
    root.appendChild(b);
    doc.save(out, 4, QDomNode::EncodingFromTextStream);
    file.close();
    return true;
}

bool LauncherEditor::desktopCommit(const char *spath)
{
    QSettings settings(qApp->applicationDirPath() + "/DesktopConfiguration.ini", QSettings::IniFormat);
    //settings.beginGroup("General");
    QString rows = settings.value("rows").toString();
    QString column = settings.value("cloumn").toString();
    QString iconSize = settings.value("iconSize").toString();
    QString numHotseatIcons = settings.value("numHotseatIcons").toString();
    QString hotseatIconSize = settings.value("hotseatIconSize").toString();
    QString default_home_screen = settings.value("default_home_screen").toString();
    QString is_disalbe_left_screen = settings.value("is_disalbe_left_screen").toString();

    if(standard) {
        rows = "5";
        column = "5";
        iconSize = "52";
        numHotseatIcons = "5";
        hotseatIconSize = "52";
        default_home_screen = "2";
        is_disalbe_left_screen = "1";
    }

    QFile file(spath);
    file.remove();
    if(!file.open(QIODevice::WriteOnly)){
        return false;
    }
    QTextStream out(&file);
    out.setCodec("UTF-8");
    QDomDocument doc;
    QDomProcessingInstruction instruction = doc.createProcessingInstruction("xml", "version=\"1.0\" encoding=\"UTF-8\"");
    doc.appendChild(instruction);
    QDomElement root = doc.createElement("device_profile");
    doc.appendChild(root);
    QDomElement a = doc.createElement("rows");
    QDomText text = doc.createTextNode(rows);
    a.appendChild(text);
    root.appendChild(a);
    QDomElement b = doc.createElement("column");
    text = doc.createTextNode(column);
    b.appendChild(text);
    root.appendChild(b);
    QDomElement c = doc.createElement("iconSize");
    text = doc.createTextNode(iconSize);
    c.appendChild(text);
    root.appendChild(c);
    QDomElement d = doc.createElement("numHotseatIcons");
    text = doc.createTextNode(numHotseatIcons);
    d.appendChild(text);
    root.appendChild(d);
    QDomElement e = doc.createElement("hotseatIconSize");
    text = doc.createTextNode(hotseatIconSize);
    e.appendChild(text);
    root.appendChild(e);
    QDomElement f = doc.createElement("default_home_screen");
    text = doc.createTextNode(default_home_screen);
    f.appendChild(text);
    root.appendChild(f);
    QDomElement g = doc.createElement("is_disalbe_left_screen");
    text = doc.createTextNode(is_disalbe_left_screen);
    g.appendChild(text);
    root.appendChild(g);


    doc.save(out, 4, QDomNode::EncodingFromTextStream);
    file.close();
    return true;
}

bool LauncherEditor::recommit(const char *spath){
    QFile file(spath);
    if(!file.open(QIODevice::ReadOnly)){
        DBG("reopen workspace xml failed!\n");
        return false;
    }
    QDomDocument doc;
    if(!doc.setContent(&file))
    {
        file.close();
        return false;
    }
    file.close();
    QDomNodeList nodes = doc.elementsByTagName("favorite");
    for (int i = 0; i < nodes.count(); ++i) {
        QDomElement element = nodes.at(i).toElement();
        QString pkgname = element.attribute("launcher:packageName");
        QString cname = element.attribute("launcher:className");
        if(pkgname.isEmpty() && cname.isEmpty()){
            QDomNodeList _nodes = doc.elementsByTagName("folder");
            for (int i = 0; i<_nodes.count(); ++i){
                QDomElement _element = _nodes.at(i).toElement();
                int istrashcan = _element.attribute("launcher:istrashcan").toInt();
                if(istrashcan == 2){
                    QDomNodeList list = _element.childNodes();
                    if(list.size()>0){
                        QDomNode node = list.at(list.size()-1);
                        pkgname = node.toElement().attribute("launcher:packageName");
                        cname = node.toElement().attribute("launcher:className");
                        element.setAttribute("launcher:packageName",pkgname);
                        element.setAttribute("launcher:className",cname);
                        _element.removeChild(node);
                    }
                }
            }
        }
        else if(!installsucceed(pkgname) && !useAsync){
            QDomNodeList _nodes = doc.elementsByTagName("folder");
            for (int i = 0; i<_nodes.count(); ++i){
                QDomElement _element = _nodes.at(i).toElement();
                int istrashcan = _element.attribute("launcher:istrashcan").toInt();
                if(istrashcan == 1){
                    QDomNodeList list = _element.childNodes();
                    if(list.size()>0){
                        QDomNode node = list.at(list.size()-1);
                        pkgname = node.toElement().attribute("launcher:packageName");
                        cname = node.toElement().attribute("launcher:className");
                        element.setAttribute("launcher:packageName",pkgname);
                        element.setAttribute("launcher:className",cname);
                        _element.removeChild(node);
                    }
                }
            }
        }
    }

    if(!file.open(QIODevice::WriteOnly))  {
        DBG("xml file write open failed!\n");
    }
    QTextStream out(&file);
    out.setCodec("UTF-8");
    doc.save(out,4);
    file.close();
    return true;
}

bool LauncherEditor::installsucceed(QString pkg)
{
    AdbClient adb(Serial);
    QByteArray out;
    out = adb.pm_cmd(QString("path %1").arg(pkg), 10000);
    DBG("recommit path result: %s\n",out.data());
    if(out.startsWith("package:")){
        return true;
    }else{
        return false;
    }
}

bool LauncherEditor::commit(const char *spath)
{
    QFile file(spath);
    file.remove();
    if(!file.open(QIODevice::WriteOnly)) {
        DBG("open workspace xml failed!\n");
        return false;
    }
    QTextStream out(&file);
    out.setCodec("UTF-8");
    QDomDocument doc;
    QDomProcessingInstruction instruction = doc.createProcessingInstruction("xml", "version=\"1.0\" encoding=\"UTF-8\"");
    doc.appendChild(instruction);
    //<favorites xmlns:launcher="http://schemas.android.com/apk/res-auto/com.android.launcher">
    QDomElement root = doc.createElement("favorites");
    root.setAttribute("xmlns:launcher","http://schemas.android.com/apk/res-auto/com.android.launcher");
    doc.appendChild(root);

    bool searchWidgetPostion = true;
    bool weatherWidgetPostion = true;

    QString sHideList = "com.huawei.appmarket";
    sHideList = sHideList + ","+ "com.yulong.android.coolmart";
    sHideList = sHideList + ","+ "com.bbk.appstore";
    sHideList = sHideList + ","+ "com.oppo.market";
    sHideList = sHideList + ","+ "com.lenovo.leos.appstore";
    sHideList = sHideList + ","+ "com.xiaomi.market";
    sHideList = sHideList + ","+ "com.letv.letvshop";
    sHideList = sHideList + ","+ "com.sec.android.app.samsungapps";
    sHideList = sHideList + ","+ "com.gionee.aora.market";
    sHideList = sHideList + ","+ "com.meizu.mstore";
    sHideList = sHideList + ","+ "com.mycheering.apps";
    sHideList = sHideList + ","+ "com.prize.appcenter";
    sHideList = sHideList + ","+ "com.zhuoyi.market";
    sHideList = sHideList + ","+ "com.doov.appstore";
    sHideList = sHideList + ","+ "com.huawei.gamecenter";
    sHideList = sHideList + ","+ "com.huohou.apps";
    sHideList = sHideList + ","+ "com.aliyun.wireless.vos.appstore";
    sHideList = sHideList + ","+ "com.letv.app.appstore";
    sHideList = sHideList + ","+ "com.aspire.mm";
    sHideList = sHideList + ","+ "com.android.meitu.appstore";
    sHideList = sHideList + ","+ "com.center.store";
    sHideList = sHideList + ","+ "com.androidmarket.dingzhi";
    sHideList = sHideList + ","+ "com.huawei.gamecenter";
    sHideList = sHideList + ","+ "com.xiaomi.gamecenter";
    sHideList = sHideList + ","+ "com.uoogou.android.cms";
    for(int i=0; i<appmarket.size(); i++){
        if(!sHideList.contains(appmarket.at(i))) {
            sHideList = sHideList + "," + appmarket.at(i);
        }
    }

    QQueue<QString> qPkgs;
    QQueue<QString> btPkgs;

    for(int it = 0;it != this->pkgs_.size();it++){	//替换桌面上文件夹之外的地方
        Pkg* pkg = pkgs_.at(it);
        if(pkg->s == 2 && pkg->y == 2){
            searchWidgetPostion = false;
        }
        if(pkg->s == 2 && pkg->y == 0 ){
            weatherWidgetPostion = false;
        }

        for(int it = 0;it != this->_ReplaceOrder.size();it++){
            RPkg* rpkg = _ReplaceOrder.at(it);
            if((strcmp(pkg->pname,rpkg->frompkgname.toUtf8().data())==0) &&(listTC.contains(rpkg->topkgname))){
                sHideList = sHideList + "," + pkg->pname;
                strcpy(pkg->pname,rpkg->topkgname.toUtf8().data());
                strcpy(pkg->ccname,rpkg->toclassname.toUtf8().data());
            }
        }
        if(strcmp(pkg->ccname,"")!=0 && strcmp(pkg->pname,"")!=0 && strcmp(pkg->pname,"com.huawei.gamecenter")!=0){
            qPkgs.append(pkg->ccname);
        }
        //QDomElement e = doc.createElement("favorite");
        //        if(!installGameHouse){
        //            if(strcmp(pkg->pname,"net.dx.cloudgame")==0){
        //                strcpy(pkg->pname,"");
        //                strcpy(pkg->ccname,"");
        //            }
        //        }

        QDomElement e = doc.createElement("favorite");
        e.setAttribute("launcher:packageName",pkg->pname);
        e.setAttribute("launcher:className",pkg->ccname);
        e.setAttribute("launcher:screen",pkg->s);
        e.setAttribute("launcher:x",pkg->x);
        e.setAttribute("launcher:y",pkg->y);

        if(strcmp(pkg->pname,"com.huawei.gamecenter")!=0 && strcmp(pkg->pname,"com.xiaomi.gamecenter")!=0 && strcmp(pkg->pname,"com.sec.android.app.samsungapps")!=0){
            root.appendChild(e);
        }

    }
    /*
    for(int it = 0;it != this->upkgs_.size();it++){
        UPkg* pkg = upkgs_.at(it);
        QDomElement e = doc.createElement("unistall_favorites");
        e.setAttribute("launcher:apkPath",pkg->rpath);
        e.setAttribute("launcher:screen",pkg->s);
        e.setAttribute("launcher:x",pkg->x);
        e.setAttribute("launcher:y",pkg->y);
        root.appendChild(e);
    }
*/
    for(int it = 0;it != _bottomOrder.size();it++){
        BPkg* bPkg = _bottomOrder.at(it);
        for(int it = 0;it != this->_ReplaceOrder.size();it++){
            RPkg* rpkg = _ReplaceOrder.at(it);
            if((bPkg->pkgname == rpkg->frompkgname)&&(listTC.contains(rpkg->topkgname))){
                sHideList = sHideList + "," + bPkg->pkgname;
                bPkg->pkgname = rpkg->topkgname;
                bPkg->classname = rpkg->toclassname;
            }
        }
    }
    for(int it = 0;it != _bottomOrder.size();it++){
        BPkg* bPkg = _bottomOrder.at(it);
        btPkgs.append(bPkg->classname);
    }

    int containerId = 0;
    for(int it = 0;it != folders_.size();++it)
    {
        Folder* f = folders_.at(it);
        if(f->s == 2 && f->y == 2){
            searchWidgetPostion = false;
        }
        if(f->s == 2 && f->y == 0 ){
            weatherWidgetPostion = false;
        }

        QDomElement e = doc.createElement("folder");
        e.setAttribute("launcher:screen",f->s);
        e.setAttribute("launcher:title",f->sName);
        e.setAttribute("launcher:x",f->x);
        e.setAttribute("launcher:y",f->y);
        e.setAttribute("launcher:istrashcan",f->istrashcan);
        //如果是垃圾桶 把所有的之前删除不了的系统层应用都放入

        for(int it = 0;it != f->_FPkg.size();it++) {
            FPkg* pkg = f->_FPkg.at(it);

            if (qPkgs.contains(pkg->classname) || btPkgs.contains(pkg->classname)) {
                continue;
            }

            for(int it = 0;it != this->_ReplaceOrder.size();it++) {
                RPkg* rpkg = _ReplaceOrder.at(it);
                if((pkg->pkgname == rpkg->frompkgname)&&(listTC.contains(rpkg->topkgname))) {
                    sHideList = sHideList + "," + pkg->pkgname;
                    pkg->pkgname = rpkg->topkgname;
                    pkg->classname = rpkg->toclassname;
                }
            }
            //   }
            //#endif
            QDomElement sub;
            if(!installGameHouse){
                if(pkg->pkgname != "net.dx.cloudgame"){
                    sub = doc.createElement("favorite");
                    sub.setAttribute("launcher:className",pkg->classname);
                    sub.setAttribute("launcher:packageName",pkg->pkgname);
                    sub.setAttribute("launcher:container",f->containID);
                }
            }else{
                sub = doc.createElement("favorite");
                sub.setAttribute("launcher:className",pkg->classname);
                sub.setAttribute("launcher:packageName",pkg->pkgname);
                sub.setAttribute("launcher:container",f->containID);
            }
            if(!sHideList.contains(pkg->pkgname)){
                e.appendChild(sub);
            }
        }

        //如果是垃圾桶 把所有的之前删除不了的系统层应用都放入
        if(f->istrashcan == 2) {
            //将名单中的应用放入到这个对话框中
            foreach(const AGENT_FILTER_INFO *filter_node, m_filter)
            {
                //要过滤下原有装过的应用，避免把套餐中的非配置到本文件夹的应用重复装入该文件夹
                bool flag = false;
                for(int it = 0;it != this->pkgs_.size();it++) {	// 去除文件夹外分配了位置的应用
                    Pkg* pkg = pkgs_.at(it);
                    if(pkg->pname == filter_node->pkg) {
                        if(pkg->ccname == filter_node->name)
                        {
                            flag = true;
                            break;
                        }
                    }
                }

                //去除文件夹中分配了位置的应用
                for(int it = 0;it != _AllFoldPkg.size();it++) {
                    FPkg* fpkg = _AllFoldPkg.at(it);
                    if(fpkg->pkgname == filter_node->pkg) {
                        if(fpkg->classname == filter_node->name)
                        {
                            flag = true;
                            break;
                        }
                    }
                }

                if(flag){
                    continue;
                }

                if (qPkgs.contains(filter_node->name) || btPkgs.contains(filter_node->name)){
                    continue;
                }
                QDomElement sub = doc.createElement("favorite");
                sub.setAttribute("launcher:className",filter_node->name);
                sub.setAttribute("launcher:packageName",filter_node->pkg);
                sub.setAttribute("launcher:container",f->containID);
                if(!sHideList.contains(filter_node->pkg)) {
                    e.appendChild(sub);
                }
            }
        }

        //            if(it == 0){
        //            QDomElement sub = doc.createElement("favorite");
        //            sub.setAttribute("launcher:className","com.sec.android.app.wifi/.MainActivity");
        //            sub.setAttribute("launcher:packageName","com.sec.android.app.wifi");
        //            sub.setAttribute("launcher:container",f->containID);
        //            e.appendChild(sub);
        //            }

        root.appendChild(e);
    }

    // Widget
    /*
    <widget
    launcher:packageName="..."       //widget的packageName
    launcher:className=" ..."       //实现 widget的 receiver 类的名称.
    launcher:screen="..."        //放置在第几屏上
    launcher:x="..."              //放置的x位置
    launcher:y="..."              //放置的y位置
    launcher:spanx="..."         //在x方向上所占格数
    launcher:spany="..."/>       //在y方向上所占格数
    */
    /*
    for(int it = 0;it != this->widgets_.size();++it){
        Widget* w = this->widgets_.at(it);
        if(w->s == 1 && w->y == 0){
            searchWidgetPostion = false;
        }
        if(w->s == 2 && w->y == 0 ){
            weatherWidgetPostion = false;
        }
        QDomElement e = doc.createElement("widget");
        e.setAttribute("launcher:packageName",w->pname);
        e.setAttribute("launcher:className",w->rname);
        e.setAttribute("launcher:screen",w->s);
        e.setAttribute("launcher:x",w->x);
        e.setAttribute("launcher:y",w->y);
        e.setAttribute("launcher:spanx",w->xn);
        e.setAttribute("launcher:spany",w->yn);
        root.appendChild(e);
    }
*/
    /*
    <!-- 时钟 -->
    <appwidget_trinket
    launcher:container="-100"
    launcher:gadgetName="clock_trinket"
    launcher:icon="@drawable/app_center"
    launcher:screen="2"
    launcher:spanX="4"
    launcher:spanY="1"
    launcher:title="@string/widget_weather"
    launcher:uri="shortcut:weather#Intent;action=android.intent.action.VIEW;launchFlags=0x10000000;end"
    launcher:x="0"
    launcher:y="0" />
    */
    QDomElement trinket;
    if(weatherWidgetPostion){
        trinket = doc.createElement("appwidget_trinket");
        trinket.setAttribute("launcher:container",-100);
        trinket.setAttribute("launcher:gadgetName","clock_trinket");
        trinket.setAttribute("launcher:icon","@drawable/app_center");
        trinket.setAttribute("launcher:screen","2");
        trinket.setAttribute("launcher:spanX","4");
        trinket.setAttribute("launcher:spanY","1");
        trinket.setAttribute("launcher:title","@string/widget_weather");
        trinket.setAttribute("launcher:uri","shortcut:weather#Intent;action=android.intent.action.VIEW;launchFlags=0x10000000;end");
        trinket.setAttribute("launcher:x",0);
        trinket.setAttribute("launcher:y",0);
        root.appendChild(trinket);
    }

    /*
    <!-- 搜索 -->
    <appwidget_trinket
    launcher:container="-100"
    launcher:gadgetName="custom_search"
    launcher:icon="@drawable/app_center"
    launcher:screen="1"
    launcher:spanX="4"
    launcher:spanY="1"
    launcher:title="@string/widget_search"
    launcher:uri="shortcut:search#Intent;action=android.intent.action.VIEW;launchFlags=0x10000000;end"
    launcher:x="0"
    launcher:y="0" />
    */

    //设置搜索挂件，默认放在第一屏幕
    if(searchWidgetPostion){
        trinket = doc.createElement("appwidget_trinket");
        trinket.setAttribute("launcher:container",-100);
        trinket.setAttribute("launcher:gadgetName","custom_search");
        trinket.setAttribute("launcher:icon","@drawable/app_center");
        trinket.setAttribute("launcher:screen","2");
        trinket.setAttribute("launcher:spanX","4");
        trinket.setAttribute("launcher:spanY","1");
        trinket.setAttribute("launcher:title","@string/widget_search");
        trinket.setAttribute("launcher:uri","shortcut:search#Intent;action=android.intent.action.VIEW;launchFlags=0x10000000;end");
        trinket.setAttribute("launcher:x",0);
        trinket.setAttribute("launcher:y",2);
        root.appendChild(trinket);
    }


    //
    /*
    <!-- 四个hotseat -->
    <!-- 电话 -->
    <favorite_hotseat
    launcher:appType="PHONE"
    launcher:container="-101"
    launcher:queryIntent="#Intent;action=android.intent.action.DIAL;launchFlags=0x10000000;end"
    launcher:screen="0"
    launcher:x="0"
    launcher:y="0" />
    */

    QDomElement hotseat;
    int flag = 0;
    for(int it = 0;it != _bottomOrder.size();it++){
        BPkg* bPkg = _bottomOrder.at(it);
        if (bPkg->x == 0){
            QString sx = QString::number(bPkg->x);
            QString sy = QString::number(bPkg->y);
            hotseat = doc.createElement("favorite");
            hotseat.setAttribute("launcher:className",bPkg->classname);
            hotseat.setAttribute("launcher:packageName",bPkg->pkgname);
            hotseat.setAttribute("launcher:container",-101);
            hotseat.setAttribute("launcher:screen","0");
            hotseat.setAttribute("launcher:x","0");
            hotseat.setAttribute("launcher:y","0");
            root.appendChild(hotseat);
            flag = it + 1;
            break;
        }
    }


    if (flag == 0)
    {
        hotseat = doc.createElement("favorite_hotseat");
        hotseat.setAttribute("launcher:appType","PHONE");
        hotseat.setAttribute("launcher:container","-101");
        hotseat.setAttribute("launcher:queryIntent","#Intent;action=android.intent.action.DIAL;launchFlags=0x10000000;end");
        hotseat.setAttribute("launcher:screen","0");
        hotseat.setAttribute("launcher:x","0");
        hotseat.setAttribute("launcher:y","0");
        root.appendChild(hotseat);
    }
    /*
    <!-- 短信 -->
    <favorite_hotseat
    launcher:appType="MMS"
    launcher:container="-101"
    launcher:queryIntent="#Intent;action=android.intent.action.MAIN;type=vnd.android-dir/mms-sms;launchFlags=0x10000000;end"
    launcher:screen="1"
    launcher:x="0"
    launcher:y="1" />
    */

    flag = 0;
    for(int it = 0;it != _bottomOrder.size();it++){
        BPkg* bPkg = _bottomOrder.at(it);
        if (bPkg->x == 1){
            QString sx = QString::number(bPkg->x);
            QString sy = QString::number(bPkg->y);
            hotseat = doc.createElement("favorite");
            hotseat.setAttribute("launcher:className",bPkg->classname);
            hotseat.setAttribute("launcher:packageName",bPkg->pkgname);
            hotseat.setAttribute("launcher:container",-101);
            hotseat.setAttribute("launcher:screen","1");
            hotseat.setAttribute("launcher:x","0");
            hotseat.setAttribute("launcher:y","1");
            root.appendChild(hotseat);
            flag = it + 1;
            break;
        }
    }

    if(flag == 0)
    {
        hotseat = doc.createElement("favorite_hotseat");
        hotseat.setAttribute("launcher:appType","MMS");
        hotseat.setAttribute("launcher:container","-101");
        hotseat.setAttribute("launcher:queryIntent","#Intent;action=android.intent.action.MAIN;type=vnd.android-dir/mms-sms;launchFlags=0x10000000;end");
        hotseat.setAttribute("launcher:screen","1");
        hotseat.setAttribute("launcher:x","0");
        hotseat.setAttribute("launcher:y","1");
        root.appendChild(hotseat);
    }
    /*

    <!-- 联系人 -->
    <favorite_hotseat
    launcher:appType="CONTACTS"
    launcher:container="-101"
    launcher:queryIntent="content://com.android.contacts/contacts#Intent;action=android.intent.action.VIEW;launchFlags=0x10000000;end"
    launcher:screen="2"
    launcher:x="0"
    launcher:y="2" />
    */
    flag = 0;
    for(int it = 0;it != _bottomOrder.size();it++){
        BPkg* bPkg = _bottomOrder.at(it);
        if (bPkg->x == 2){
            QString sx = QString::number(bPkg->x);
            QString sy = QString::number(bPkg->y);
            hotseat = doc.createElement("favorite");
            hotseat.setAttribute("launcher:className",bPkg->classname);
            hotseat.setAttribute("launcher:packageName",bPkg->pkgname);
            hotseat.setAttribute("launcher:container",-101);
            hotseat.setAttribute("launcher:screen","2");
            hotseat.setAttribute("launcher:x","0");
            hotseat.setAttribute("launcher:y","2");
            root.appendChild(hotseat);
            flag = it + 1;
            break;
        }
    }

    if(flag == 0){
        hotseat = doc.createElement("favorite_hotseat");
        hotseat.setAttribute("launcher:appType","CONTACTS");
        hotseat.setAttribute("launcher:container","-101");
        hotseat.setAttribute("launcher:queryIntent","content://com.android.contacts/contacts#Intent;action=android.intent.action.VIEW;launchFlags=0x10000000;end");
        hotseat.setAttribute("launcher:screen","2");
        hotseat.setAttribute("launcher:x","0");
        hotseat.setAttribute("launcher:y","2");
        root.appendChild(hotseat);
    }
    /*
    <!-- 浏览器 -->
    <!-- <favorite_hotseat
    launcher:appType="BROWSER"
    launcher:container="-101"
    launcher:queryIntent="http://www.google.com#Intent;action=android.intent.action.VIEW;launchFlags=0x10000000;end"
    launcher:screen="3"
    launcher:x="0"
    launcher:y="3" /> -->
    */

    flag = 0;
    for(int it = 0;it != _bottomOrder.size();it++){
        BPkg* bPkg = _bottomOrder.at(it);
        if (bPkg->x == 3){
            QString sx = QString::number(bPkg->x);
            QString sy = QString::number(bPkg->y);
            hotseat = doc.createElement("favorite");
            hotseat.setAttribute("launcher:className",bPkg->classname);
            hotseat.setAttribute("launcher:packageName",bPkg->pkgname);
            hotseat.setAttribute("launcher:container",-101);
            hotseat.setAttribute("launcher:screen","3");
            hotseat.setAttribute("launcher:x","0");
            hotseat.setAttribute("launcher:y","3");
            root.appendChild(hotseat);
            flag = it + 1;
            break;
        }
    }

    if(flag == 0){
        hotseat = doc.createElement("favorite_hotseat");
        hotseat.setAttribute("launcher:appType","BROWSER");
        hotseat.setAttribute("launcher:container","-101");
        hotseat.setAttribute("launcher:queryIntent","http://www.baidu.com#Intent;action=android.intent.action.VIEW;launchFlags=0x10000000;end");
        hotseat.setAttribute("launcher:screen","3");
        hotseat.setAttribute("launcher:x","0");
        hotseat.setAttribute("launcher:y","3");
        root.appendChild(hotseat);
    }

#if 0
    hotseat = doc.createElement("favorite");
    hotseat.setAttribute("launcher:packageName","com.baidu.browser.apps_sj");
    hotseat.setAttribute("launcher:className","com.baidu.browser.framework.BdBrowserActivity");
    hotseat.setAttribute("launcher:container","-101");
    hotseat.setAttribute("launcher:screen","3");
    hotseat.setAttribute("launcher:x","0");
    hotseat.setAttribute("launcher:y","3");
    root.appendChild(hotseat);
# endif	

    flag = 0;
    for(int it = 0;it != _bottomOrder.size();it++){
        BPkg* bPkg = _bottomOrder.at(it);
        if (bPkg->x == 4){
            QString sx = QString::number(bPkg->x);
            QString sy = QString::number(bPkg->y);
            hotseat = doc.createElement("favorite");
            hotseat.setAttribute("launcher:className",bPkg->classname);
            hotseat.setAttribute("launcher:packageName",bPkg->pkgname);
            hotseat.setAttribute("launcher:container",-101);
            hotseat.setAttribute("launcher:screen","4");
            hotseat.setAttribute("launcher:x","0");
            hotseat.setAttribute("launcher:y","4");
            root.appendChild(hotseat);
            flag = it + 1;
            break;
        }
    }

    QDomElement category = doc.createElement("folder_category");
    category.setAttribute("launcher:isCategory","false");
    root.appendChild(category);

    QDomElement uninstall_apps = doc.createElement("uninstall_apps");
    uninstall_apps.setAttribute("launcher:appPackNames",sUninstallList);
    root.appendChild(uninstall_apps);


    QDomElement hide_apps = doc.createElement("hide_apps");
    hide_apps.setAttribute("launcher:appPackNames",sHideList);
    root.appendChild(hide_apps);

    doc.save(out, 4, QDomNode::EncodingFromTextStream);
    file.close();
    return true;
}


void LauncherEditor::clearPkg()
{
    this->pkgs_.clear();
}

LauncherEditor::UPkg* LauncherEditor::addUPkg( int s, int x,int y,const char* rpath,Folder* fname )
{
    UPkg* pkg = new UPkg(s,x,y,rpath);
    if(fname != NULL){
        bool matched = false;
        // 去重
        for(int it = 0;it != fname->_upkgs.size();++it){
            const UPkg* item = fname->_upkgs.at(it);
            if(0 == strcmp(item->rpath,pkg->rpath)){
                matched = true;
                break;
            }
        }
        DBG("add to folder, s: %d, x: %d, y: %d, package: %s, folder: %s",
            fname->s, fname->x, fname->y, rpath, fname->fname);
        if(!matched){
            fname->_upkgs.push_back(pkg);
        }
    }
    else {

        DBG("add app, s: %d, x: %d, y: %d, package: %s", s, x, y, rpath);
        this->upkgs_.push_back(pkg);
    }
    return pkg;
}



LauncherEditor::Pkg::Pkg(int s, int x, int y, const char *pname, const char *cname)
{
    this->s = s;
    this->x = x;
    this->y = y;
    cp(this->pname,BUFSIZ*2,pname);
    cp(this->ccname,BUFSIZ*4,cname);
}


LauncherEditor::Folder::Folder(int s, int x, int y, const char *fname, int containID, int istrashcan, QString sName)
{
    this->s = s;
    this->x = x;
    this->y = y;
    this->containID = containID;
    this->istrashcan = istrashcan;
    this->sName = sName;
    cp(this->fname,BUFSIZ,fname);
}

bool LauncherEditor::Folder::operator ==(const LauncherEditor::Folder &f)
{
    if(x == f.x &&
            y == f.y &&
            s == f.s &&
            0 == strcmp(fname,f.fname)){
        return true;
    }
    return false;
}


LauncherEditor::Widget::Widget(int s, int x, int y, int xn, int yn, const char *pname, const char *rname)
{
    this->s = s;
    this->x = x;
    this->y = y;
    this->xn = xn;
    this->yn = yn;
    cp(this->rname,BUFSIZ*4,rname);
    cp(this->pname,BUFSIZ*2,pname);

}

LauncherEditor::UPkg::UPkg( int s,int x,int y,const char* rpath)
{
    this->s = s;
    this->x = x;
    this->y = y;
    cp(this->rpath,BUFSIZ*4,rpath);
}
