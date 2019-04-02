#ifndef FORMBUNDLENODE_H
#define FORMBUNDLENODE_H

#include <QWidget>

namespace Ui {
class FormBundleNode;
}

class Bundle;
class FormBundleNode : public QWidget
{
    Q_OBJECT
signals:
    void signal_bundleClicked(Bundle *b, bool checked);

public:
    explicit FormBundleNode(Bundle *bundle, QWidget *parent = 0);
    ~FormBundleNode();

    Bundle *bundle;

    bool isChecked();
    void setChecked(bool b);

    void setIcon(int index);

public slots:
    void slot_refreshUI(Bundle *b);
    void slot_bundleClicked(bool checked);

private:
    Ui::FormBundleNode *ui;
};

#endif // FORMBUNDLENODE_H
