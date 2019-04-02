#ifndef _WIN_VERIFY_TRUST_H_
#define _WIN_VERIFY_TRUST_H_

#include <windows.h>
#include <QVector>
#include <QString>


class WinVerifyTrust {
public:
    // 验证驱动文件签名, 支持 .sys .dll .cat .exe 文件
    // 返回失败可能是没有签名, 或签名未被windows验证通过
    static bool verifyFile(PCWSTR fileName);

    // 返回PE依赖文件(全路径名)列表, 默认递归深度为 3
    static QVector<QString> getPEDependencyFiles(PCWSTR filename, int deep = 3);

    // 返回PE导入列表, 支持 .sys .dll .exe
    static QVector<QString> getPEImportList(PCWSTR filename);

private:
    WinVerifyTrust() {}
};


#endif // _WIN_VERIFY_TRUST_H_
