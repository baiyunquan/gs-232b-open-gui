#include "ConsolePanel.h"

#include <QHBoxLayout>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

ConsolePanel::ConsolePanel(QWidget *parent) : QGroupBox(tr("CONSOLE"), parent)
{
    setObjectName(QStringLiteral("consolePanel"));
    auto *layout = new QVBoxLayout(this);
    auto *output = new QPlainTextEdit(this);
    output->setObjectName(QStringLiteral("consoleOutput"));
    output->setReadOnly(true);
    output->setLineWrapMode(QPlainTextEdit::NoWrap);
    layout->addWidget(output, 1);

    auto *inputRow = new QHBoxLayout;
    m_input = new QLineEdit(this);
    m_input->setObjectName(QStringLiteral("commandInput"));
    m_input->setPlaceholderText(tr("Enter command"));
    m_send = new QPushButton(tr("Send"), this);
    m_send->setObjectName(QStringLiteral("sendButton"));
    inputRow->addWidget(m_input, 1);
    inputRow->addWidget(m_send);
    layout->addLayout(inputRow);
    setCommandEnabled(false);

    // TODO: 监听实际串口/WS 收发原始字节；所有按钮命令、响应及轮询都从同一真实流打印。
    // TODO: 控制台获得焦点时允许直接键入；Enter 与输入框发送均仅编码为一个 CR。
    // TODO: 接入传输后端后启用终端输入，禁止编辑历史响应或将本地输入伪装为设备响应。
}

void ConsolePanel::setCommandEnabled(bool enabled)
{
    m_input->setEnabled(enabled);
    m_send->setEnabled(enabled);
}
