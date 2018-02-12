/**
 * Copyright (C) 2016 - Marcos Cardinot
 * @author Marcos Cardinot <mcardinot@gmail.com>
 */

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QVector>
#include <QStringList>
#include <QTextStream>
#include <set>

#include "project.h"
#include "experiment.h"
#include "utils.h"

namespace evoplex
{

Project::Project(MainApp* mainApp, int id, QString& error, const QString& filepath)
    : m_mainApp(mainApp)
    , m_id(id)
    , m_filepath(filepath)
    , m_lastExpId(-1)
{
    setFilePath(filepath);
    if (!filepath.isEmpty()) {
        this->blockSignals(true);
        importExperiments(filepath, error);
        this->blockSignals(false);
    }
    m_hasUnsavedChanges = false;
}

Project::~Project()
{
    Utils::deleteAndShrink(m_experiments);
}

void Project::setFilePath(const QString& path)
{
    m_filepath = path;
    m_name = path.isEmpty() ? QString("Project%1").arg(m_id)
                            : QFileInfo(path).baseName();
}

void Project::playAll()
{
    QHash<int, Experiment*>::iterator it;
    for (it = m_experiments.begin(); it != m_experiments.end(); ++it)
        it.value()->play();
}

Experiment* Project::newExperiment(Experiment::ExperimentInputs* inputs)
{
    if (!inputs) {
        return nullptr;
    }

    ++m_lastExpId;
    Experiment* exp = new Experiment(m_mainApp, m_lastExpId, inputs, this);
    m_experiments.insert(m_lastExpId, exp);

    m_hasUnsavedChanges = true;
    emit (hasUnsavedChanges(m_hasUnsavedChanges));
    emit (expAdded(m_lastExpId));
    return exp;
}

bool Project::editExperiment(int expId, Experiment::ExperimentInputs* newInputs)
{
    Experiment* exp = m_experiments.value(expId);
    Q_ASSERT(exp);
    if (!exp->init(newInputs)) {
        return false;
    }
    m_hasUnsavedChanges = true;
    emit (hasUnsavedChanges(m_hasUnsavedChanges));
    emit (expEdited(expId));
    return true;
}

const int Project::importExperiments(const QString& filePath, QString& errorMsg)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        errorMsg = "Couldn't read the experiments from:\n`" + filePath + "`";
                 + "Please, make sure it is a readable csv file.";
        qWarning() << "[Project]: " << errorMsg;
        return 0;
    }

    QTextStream in(&file);

    // read header
    const QStringList header = in.readLine().split(",");
    if (header.isEmpty()) {
        errorMsg = "Couldn't read the experiments from:\n`" + filePath + "`"
                 + "\nThe header must have: " + m_mainApp->generalAttrSpace().keys().join(", ");
        qWarning() << "[Project]: " << errorMsg;
        return 0;
    }

    // import experiments
    int row = 1;
    while (!in.atEnd()) {
        const QStringList values = in.readLine().split(",");
        QString expErrorMsg;
        Experiment::ExperimentInputs* inputs = Experiment::readInputs(m_mainApp, header, values, expErrorMsg);
        if (!inputs || !newExperiment(inputs)) {
            errorMsg = QString("Couldn't read the experiment at line %1 from:\n`%2`\n"
                               "Error: %3").arg(row).arg(filePath).arg(expErrorMsg);
            qWarning() << "[Project]: " << errorMsg;
            delete inputs;
            file.close();
            return row - 1;
        }
        ++row;
    }
    file.close();

    if (row == 1) {
        errorMsg = QString("This file is empty.\nThere were no experiments to be read.\n%1").arg(filePath);
        qWarning() << "[Project]: " << errorMsg;
        return 0;
    }

    return row - 1;
}

bool Project::saveProject(QString& errMsg, std::function<void(int)>& progress)
{
    progress(0);
    if (m_experiments.empty()) {
        errMsg = QString("Unable to save the project '%1'.\n"
                "This project is empty. There is nothing to save.").arg(name());
        qWarning() << "[Project]" << errMsg;
        return false;
    }

    progress(5);
    QFile file(m_filepath);
    QFileInfo fi(file);
    if (fi.suffix() != "csv" || !file.open(QFile::WriteOnly | QFile::Truncate)) {
        errMsg = QString("Unable to save the project '%1'.\n"
                "Please, make sure the path below corresponds to a writable csv file!\n%2")
                .arg(name()).arg(m_filepath);
        qWarning() << "[Project]" << errMsg;
        return false;
    }

    progress(10);
    std::vector<QString> header;
    for (Experiment* exp : m_experiments) {
        const Experiment::ExperimentInputs* inputs = exp->inputs();
        std::vector<QString> general = inputs->generalAttrs->names();
        header.insert(header.end(), general.begin(), general.end());
        // prefix all model attributes with the modelId
        foreach (const QString& attrName, inputs->modelAttrs->names()) {
            header.emplace_back(exp->modelId() + "_" + attrName);
        }
        // prefix all graph attributes with the graphId
        foreach (const QString& attrName, inputs->graphAttrs->names()) {
            header.emplace_back(exp->graphId() + "_" + attrName);
        }
    }
    // remove duplicates
    progress(25);
    std::set<QString> s(header.begin(), header.end());
    header.assign(s.begin(), s.end());

    progress(30);
    QTextStream out(&file);
    for (int i = 0; i < header.size()-1; ++i) {
        out << header.at(i) << ",";
    }
    out << header.at(header.size()-1) << "\n";

    progress(35);
    for (Experiment* exp : m_experiments) {
        const Experiment::ExperimentInputs* inputs = exp->inputs();
        const QString modelId_ = exp->modelId() + "_";
        const QString graphId_ = exp->graphId() + "_";

        QStringList values;
        foreach (QString attrName, header) {
            int idx = inputs->generalAttrs->indexOf(attrName);
            if (idx != -1) {
                values.append(inputs->generalAttrs->value(idx).toQString());
            } else if (attrName.startsWith(modelId_)) {
                idx = inputs->modelAttrs->indexOf(attrName.remove(modelId_));
                if (idx != -1) values.append(inputs->modelAttrs->value(idx).toQString());
            } else if (attrName.startsWith(graphId_)) {
                idx = inputs->graphAttrs->indexOf(attrName.remove(graphId_));
                if (idx != -1) values.append(inputs->graphAttrs->value(idx).toQString());
            } else {
                values.append(""); // not found; leave empty
            }
        }
        out << values.join(",") << "\n";
    }
    file.close();
    m_hasUnsavedChanges = false;
    emit (hasUnsavedChanges(false));
    progress((100));
    qWarning() << "[Project]: project has been saved!" << name();
    return true;
}

}
