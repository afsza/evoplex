/**
 * Copyright (C) 2017 - Marcos Cardinot
 * @author Marcos Cardinot <mcardinot@gmail.com>
 */

#ifndef TITLEBAR_H
#define TITLEBAR_H

#include <QDockWidget>
#include <QWidget>

#include "core/experiment.h"

class Ui_TitleBar;

namespace evoplex {

class TitleBar : public QWidget
{
    Q_OBJECT

public:
    explicit TitleBar(Experiment* exp, QDockWidget* parent);
    ~TitleBar();

    void changeCurrentTrial(int trialId);

signals:
    void trialSelected(int);
    void openSettingsDlg();

private slots:
    void slotTrialChanged(int trialId);

protected:
    virtual void paintEvent(QPaintEvent* pe);

private:
    Ui_TitleBar* m_ui;
    Experiment* m_exp;

    bool isFreeToUse();
};
}

#endif // TITLEBAR_H