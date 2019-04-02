#include "launchermaker.h"
#include "launcherconfig.h"
#include "launchereditor.h"
#include "zlog.h"
#include <QUuid>
#include <QProcess>
#include <stdio.h>
#include <QFile>
#include "zstringutil.h"

LauncherMaker::LauncherMaker(LauncherConfig *config, const char *serial)
    :_config(config),_serial(serial)
{
    _editor = new LauncherEditor();

}

void LauncherMaker::init()
{
    // 文件夹信息
    for(int it = 0;it != _config->_folderOrder.size();++it){
        const PosNode& n = _config->_folderOrder.at(it);
        _editor->addFolder(n.screen,n.x,n.y,n.name.toUtf8().data(),n.containID,n.istrashcan,n.name);
    }
    // 系统挂件信息
    for(int it = 0;it != _config->_widgetOrder.size();++it){
        const PosNode& n = _config->_widgetOrder.at(it);
        _editor->addWidget(n.screen,n.x,n.y,n.xn,n.yn,n.pname.toUtf8().data(),n.cname.toUtf8().data());
    }
    // 固定应用位置信息
    for(int it = 0;it != _config->_fixedPkg.size();++it){
        const PosNode& n = _config->_fixedPkg.at(it);
        _editor->addPkg(n.screen,n.x,n.y,n.pname.toUtf8().data(),n.cname.toUtf8().data());
    }
}

bool LauncherMaker::sync()
{
    _editor->sUninstallList = _config->sUninstallList;

    DBG("fool.\n");
    int currentPos = 0;
    _editor->clearPkg();
    // 固定应用位置信息
    DBG("clear _editor,do with fixedpkg.\n");
    for(int it = 0;it != _config->_fixedPkg.size();++it){
        const PosNode& n = _config->_fixedPkg.at(it);
        _editor->addPkg(n.screen,n.x,n.y,n.pname.toUtf8().data(),n.cname.toUtf8().data());
    }
    DBG("_fixedPkg.size = %d.\n",_config->_fixedPkg.size());

    QFile file("ini.txt");
    file.resize(0);

    foreach(const AGENT_FILTER_INFO *filter_node,_editor->m_filter){
        QString label = filter_node->label;
        if(label.contains(ZStringUtil::fromGBK("商店")) || label.contains(ZStringUtil::fromGBK("市场"))){
            _editor->appmarket.append(filter_node->pkg);
        }
        for(int it = 0;it != _config->_replaceOrder.size();++it){
            const ReplaceNode& rapk = _config->_replaceOrder.at(it);
            QStringList list = rapk.lName.split(",");
            if(list.size() > 0){
                for(int j=0;j<list.size();j++){
                    if(list.at(j) == label){
                        RPkg *rpkg = new RPkg;
                        rpkg->fromclassname = filter_node->name;
                        rpkg->frompkgname = filter_node->pkg;
                        rpkg->toclassname = rapk.classname;
                        rpkg->topkgname = rapk.pkgname;
                        _editor->_ReplaceOrder.append(rpkg);
                        if(file.open(QFile::WriteOnly | QFile::Text | QFile::Append)){
                            QString str = rpkg->toclassname + "&" +list.at(j) +"\n";
                            file.write(str.toUtf8().data());
                            file.close();
                        }
                    }
                }
            }
        }
    }
    QStringList listTC;
    if(!_config->_posOrder.isEmpty()){
        PosNode node = _config->_posOrder.at(currentPos);
        int PosOrderSize = _config->_posOrder.size();
        //按照后台设置的apk顺序将安装的应用(后台已指定的)排序
        //注释是因为杭州这边所有位置都是预先指定的，避免发生交叉覆盖
        while(currentPos < PosOrderSize)
        {
            node = _config->_posOrder.at(currentPos);
            if (node.containID == -101){
                //给底部四大天王用的
                BPkg *bPkg = new BPkg;
                bPkg->x = node.x;
                bPkg->y = node.y;

                if(node.type == "SYS"){
                    foreach(const AGENT_FILTER_INFO *filter_node,_editor->m_filter)
                    {
                        QString label = filter_node->label;
                        QString name = node.name;
                        QStringList namelist = name.split(",");
                        if(namelist.size()>0){
                            for(int j = 0;j<namelist.size();j++){
                                if (label == namelist.at(j)){
                                    node.cname = filter_node->name.toUtf8().data();
                                    node.pname = filter_node->pkg.toUtf8().data();
                                    break;
                                }
                            }
                        }
                    }
                }

                bPkg->classname = node.cname;
                bPkg->pkgname = node.pname;

                _editor->_bottomOrder.append(bPkg);
            }else if(node.type == "SYS"){
                // 手机出厂原生所带的应用，一般在系统层
                QStringList Label;
                QString market = "900市场,华为应用市场,应用市场,三星应用商店,应用商店,软件商店,小米商城";
                QString game = "华为游戏中心,游戏中心,游戏大厅,游戏";

                foreach(const AGENT_FILTER_INFO *filter_node,_editor->m_filter)
                {
                    QString label = filter_node->label;
                    Label.append(label);
                    QString name = node.name;
                    QStringList namelist = name.split(",");
                    if(namelist.size()>0){
                        for(int j = 0;j<namelist.size();j++){
                            if (label == namelist.at(j)){
                                int a = 221;
                                _editor->addPkg(node.screen,node.x,node.y,filter_node->pkg.toUtf8().data(),filter_node->name.toUtf8().data());
                                node.pname = filter_node->pkg;
                                node.cname = filter_node->name;
                                break;
                            }
                            QStringList mlist = market.split(",");
                            if(mlist.size()>0){
                                for(int i=0; i<mlist.size(); i++){
                                    QString Market = mlist.at(i);
                                    if(namelist.at(j) == ZStringUtil::fromGBK(Market.toLatin1())){
                                        QString cname = "com.zxly.market.activity.SplashActivity";
                                        QString pname = "com.shyz.desktop";
                                        _editor->addPkg(node.screen,node.x,node.y,pname.toUtf8().data(),cname.toUtf8().data());
                                        node.pname = pname;
                                        node.cname = cname;
                                    }
                                }
                            }
                            QStringList glist = game.split(",");
                            if(glist.size()>0){
                                for(int i=0; i<glist.size(); i++){
                                    QString Game = glist.at(i);
                                    if(namelist.at(j) == ZStringUtil::fromGBK(Game.toLatin1())){
                                        QString cname = "com.shyz.desktop.activity.GameCenterActivity";
                                        QString pname = "com.shyz.desktop";
                                        _editor->addPkg(node.screen,node.x,node.y,pname.toUtf8().data(),cname.toUtf8().data());
                                        node.pname = pname;
                                        node.cname = cname;
                                    }
                                }
                            }
                        }
                    }
                }
                QString name = node.name;
                QStringList namelist = name.split(",");
                if(namelist.size() == 1){
                    if(!Label.contains(namelist.at(0))){
                        _editor->addPkg(node.screen,node.x,node.y,node.pname.toUtf8().data(),node.cname.toUtf8().data());
                    }
                }
                int flag = 0;
                if(namelist.size()>1){
                    for(int it=0 ;it!=namelist.size(); ++it){
                        if(Label.contains(namelist.at(it))){
                            flag = 1;
                        }
                    }
                    if(flag == 0){
                        _editor->addPkg(node.screen,node.x,node.y,node.pname.toUtf8().data(),node.cname.toUtf8().data());
                    }
                }
            }else{
                //
                _editor->addPkg(node.screen,node.x,node.y,node.pname.toUtf8().data(),node.cname.toUtf8().data());

            }
            currentPos ++;
            if((node.cname == "")||(node.pname == "")){
                int a = 21;
            }
        }

        //
        for(int it = 0; it != _hiddenOrder.size();++it){
            const HiddenNode& apk = _hiddenOrder.at(it);
            for(int it = 0;it != _editor->folders_.size();++it){
                LauncherEditor::Folder* f = _editor->folders_.at(it);
                if(f->istrashcan == 1){
                    FPkg *fpkg = new FPkg;
                    fpkg->classname = apk.classname;
                    fpkg->pkgname = apk.pkgname;
                    f->_FPkg.append(fpkg);
                    _editor->hiddenPkg.append(apk.pkgname);
                    _editor->_AllFoldPkg.append(fpkg);
                    break;
                }
            }
        }
        //_pkgOrder的内容是用于处理文件夹中的内容 因为_posOrder上一般已经直接提供了包名和类名
        QString s_sim = "";
        for(int it = 0;it != _config->_pkgOrder.size();++it){
            const PkgNode& apk = _config->_pkgOrder.at(it);
            listTC.append(apk.pkgname);
            if(_apps.contains(apk.pkgname)){//必须是装过的
                QString  key = _apps.value(apk.pkgname);
                if (key == ""){
                    continue;
                }
                if(apk.inside){
                    for(int it = 0;it != _editor->folders_.size();++it)
                    {
                        LauncherEditor::Folder* f = _editor->folders_.at(it);
                        if(apk.foldername == f->sName)
                        {
                            FPkg *fpkg = new FPkg;
                            fpkg->classname = key;
                            fpkg->pkgname = apk.pkgname;
                            f->_FPkg.append(fpkg);
                            _editor->_AllFoldPkg.append(fpkg);
                        }
                    }
                }else if(!apk.inside)
                {
                    for(int it = 0;it != _editor->folders_.size();++it)
                    {
                        LauncherEditor::Folder* f = _editor->folders_.at(it);
                        if(f->istrashcan == 1)
                        {
                            FPkg *fpkg = new FPkg;
                            fpkg->classname = key;
                            fpkg->pkgname = apk.pkgname;
                            f->_FPkg.append(fpkg);
                            _editor->_AllFoldPkg.append(fpkg);
                            break;
                        }
                    }
                }
            }else if(apk.inside){ //这种情况是表示，该应用是用中文名显示的系统自带应用
                for(int it = 0;it != _editor->folders_.size();++it) //这个循环是为了按文件夹分类存放文件
                {
                    LauncherEditor::Folder* f = _editor->folders_.at(it);
                    if(apk.foldername == f->sName)
                    {
                        FPkg *fpkg = new FPkg;
                        int sim_flag = 0;
                        foreach(const AGENT_FILTER_INFO *filter_node,_editor->m_filter)
                        {
                            QString label = filter_node->label;
                            if (label == apk.pkgname){
                                if(apk.pkgname == ZStringUtil::fromGBK("SIM 卡应用")) //去重
                                {
                                    if(s_sim == "")
                                    {
                                        s_sim = filter_node->name;  //类名去重
                                    }else if(filter_node->name == s_sim){
                                        continue;
                                    }
                                }

                                fpkg->classname = filter_node->name;
                                fpkg->pkgname = filter_node->pkg;
                                break;
                            }
                            else if((apk.pkgname == ZStringUtil::fromGBK("SIM 卡应用1"))
                                    &&(label == ZStringUtil::fromGBK("SIM 卡应用"))){ //做一个去重复判断，
                                if(s_sim == "")
                                {
                                    s_sim = filter_node->name;  //类名去重
                                }else if(filter_node->name == s_sim){
                                    continue;
                                }

                                fpkg->classname = filter_node->name;
                                fpkg->pkgname = filter_node->pkg;
                                break;
                            }
                        }
                        if(fpkg->pkgname!=""){  //这种现象出现的原因是因为像QQ这种，没有设置卸载阶段的防卸载白名单，导致应用在配置阶段已经不存在导致
                            f->_FPkg.append(fpkg);
                            _editor->_AllFoldPkg.append(fpkg);
                        }
                    }
                }

            }


        }


        //这里做填充空洞的操作
        if( currentPos != PosOrderSize ){
            //首先从标志位垃圾桶的地方填入 比如outapps
            QStringList keys = _config->_trashcanapps.keys();
            foreach(const QString& key,keys){
                _editor->addPkg(node.screen,node.x,node.y,key.toUtf8().data(), _config->_trashcanapps.value(key).toUtf8().data());
                currentPos ++;
                if(currentPos != PosOrderSize){
                    node = _config->_posOrder.at(currentPos);
                }
                else {
                    break;
                }
            }
        }

        DBG("_outapps!");
        // 如果 配置的位置未被填充满，从备选的_outapps中插入
        if( currentPos != PosOrderSize ){
            QStringList keys = _outapps.keys();
            foreach(const QString& key,keys){
                _editor->addPkg(node.screen,node.x,node.y,key.toUtf8().data(),_outapps.value(key).toUtf8().data());
                currentPos ++;
                if(currentPos != PosOrderSize){
                    node = _config->_posOrder.at(currentPos);
                }
                else {
                    break;
                }
            }
        }
        DBG("_pushapps!");
        // 如果 配置的位置未被填充满，从_pushapps中插入
        if( currentPos != PosOrderSize ){
            QStringList keys = _pushapps.keys();
            foreach(const QString& key,keys){
                _editor->addUPkg(node.screen,node.x,node.y,_pushapps.value(key).toUtf8().data());
                currentPos ++;
                if(currentPos != PosOrderSize){
                    node = _config->_posOrder.at(currentPos);
                }
                else {
                    break;
                }
            }
        }

        if(!_editor->validate()){ //要求三屏都有图标.
            //DBG("Desktop config is invalid!");
            //return false;
        }
    }
    else {
        DBG("Desktop config is empty!");
        return false;
    }

    DBG("Desktop config is not empty.\n");
    QString configname = QUuid::createUuid().toString() + "_workspace.xml";
    m_configName = configname;
    _editor->autoFolder(false);
    //装入一个套餐应用的列表，以用于控制
    _editor->listTC = listTC;
    _editor->Brand = _config->Brand;
    DBG("%s begin to commit!\n",configname.toUtf8().data());
    _editor->commit(configname.toUtf8().data());
    _editor->recommit(configname.toUtf8().data());

    QString ini = QUuid::createUuid().toString() + "_desktopini.xml";
    m_ini = ini;
    _editor->desktopCommit(ini.toUtf8().data());

    QString hiddenConf = QUuid::createUuid().toString() + "_config.xml";
    m_hiddenConf = hiddenConf;
    _editor->hiddenCommit(hiddenConf.toUtf8().data());

    return true;
}



bool LauncherMaker::install(const char *package, const char *path)
{
    bool inside = false,contains = false;
    /*
    if(!_config->contains(package)){
    return false;
    }
    */
    contains = _config->contains(package);
    DBG("add install apk: %s %s", package, path);
    QString stag = "launchable-activity: name='";
    QString line = "aapt d badging %1 | findstr launchable-activity: name='";
    QString className = "";
    QString command = line.arg(path);
    QProcess cmd;
    cmd.start(command);;
    if(cmd.waitForStarted()){
        if(cmd.Running == cmd.state()){
            cmd.waitForFinished();
        }
    }
    QString output = cmd.readAllStandardOutput();
    int idx = output.indexOf(stag);
    if(idx != -1 ){
        int len = output.indexOf("'",idx+stag.length()) - (idx + stag.length());
        className = output.mid(idx + stag.length(),len);
    }
    QString folderName = "";
    inside = _config->inside(package,folderName);
    if(inside){
        LauncherEditor::Folder* f = _editor->search(folderName.toLocal8Bit().data());
        if(f != NULL){
            DBG("add install apk: %s %s, folder: %s", package, path, f->fname);
            _editor->addPkg(0,0,0,package,className.toUtf8().data(),f);
        }
        else
        {
            DBG("add install apk failed: %s %s, folder: %s", package, path, folderName.toUtf8().data());
        }
    }
    else {
        //        if(_curPos != _config->size()){
        //            const LauncherConfig::PosNode& n = _config->at(_curPos);
        //            _editor->addPkg(1,1,1,package,className.toUtf8().data());
        //            _curPos;
        //        }
        DBG("add install apk:contains:%s, %s %s",contains?"Y":"N",package, path);
        if(contains){
            _apps.insert(package,className);
        }
        else {
            // 如果APK未在 后台的序列中配置，先保留备用。。
            _outapps.insert(package,className);
        }
    }
    return true;
}

void LauncherMaker::folder( bool auto_ /*= true*/ )
{
    _editor->autoFolder(auto_);
}


bool LauncherMaker::push( const char* package,const char* rpath )
{
    bool inside = false,contains = false;
    /*
    if(!_config->contains(package)){
    return false;
    }
    */
    contains = _config->contains(package);
    DBG("add push apk: %s %s", package, rpath);
    QString folderName = "";
    inside = _config->inside(package,folderName);
    if(inside){
        LauncherEditor::Folder* f = _editor->search(folderName.toLocal8Bit().data());
        if(f != NULL){
            DBG("add push apk: %s %s, folder: %s", package, rpath, f->fname);
            _editor->addUPkg(0,0,0,rpath,f);
        }
        else
        {
            DBG("add push apk failed: %s %s, folder: %s", package, rpath, folderName.toUtf8().data());
        }
    }
    else {
        //        if(_curPos != _config->size()){
        //            const LauncherConfig::PosNode& n = _config->at(_curPos);
        //            _editor->addPkg(1,1,1,package,className.toUtf8().data());
        //            _curPos;
        //        }
        DBG("add push apk:contains:%s, %s %s",contains?"Y":"N",package, rpath);
        _pushapps.insert(package,rpath);
    }
    return true;
}

void LauncherMaker::InsertAPK(QString package,QString ClassName)
{
    _apps.insert(package,ClassName);
}
