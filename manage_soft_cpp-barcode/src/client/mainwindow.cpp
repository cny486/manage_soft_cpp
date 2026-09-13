#include "mainwindow.h"

#include "accountsecuritydialog.h"
#include "appservice.h"
#include "appschema.h"
#include "managementpage.h"
#include "reimbursementoverviewpage.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QDate>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStyle>
#include <QVariantMap>
#include <QVBoxLayout>
#include <QWidget>

namespace {
PageConfig inventoryConfig()
{
    return {
        QStringLiteral("inventory"),
        QStringLiteral("库存管理"),
        {
            {QStringLiteral("number"), QStringLiteral("No."), FieldType::Text, false, {}},
            {QStringLiteral("quantity"), QStringLiteral("Quantity"), FieldType::Integer, true, {}},
            {QStringLiteral("date"), QStringLiteral("日期"), FieldType::Date, false, {}},
            {QStringLiteral("unit"), QStringLiteral("单位"), FieldType::Text, false, {}},
            {QStringLiteral("location"), QStringLiteral("存储位置"), FieldType::Text, false, {}},
            {QStringLiteral("comment"), QStringLiteral("Comment"), FieldType::Multiline, false, {}},
            {QStringLiteral("designator"), QStringLiteral("Designator"), FieldType::Text, false, {}},
            {QStringLiteral("footprint"), QStringLiteral("Footprint"), FieldType::Text, false, {}},
            {QStringLiteral("value"), QStringLiteral("Value"), FieldType::Text, false, {}},
            {QStringLiteral("manufacturerPart"), QStringLiteral("Manufacturer Part"), FieldType::Text, true, {}},
            {QStringLiteral("manufacturer"), QStringLiteral("Manufacturer"), FieldType::Text, false, {}},
            {QStringLiteral("addIntoBom"), QStringLiteral("Add into BOM"), FieldType::Text, false, {}},
            {QStringLiteral("convertToPcb"), QStringLiteral("Convert to PCB"), FieldType::Text, false, {}},
            {QStringLiteral("pinCount"), QStringLiteral("Pin Count"), FieldType::Integer, false, {}},
            {QStringLiteral("category"), QStringLiteral("Category"), FieldType::Text, false, {}},
            {QStringLiteral("device"), QStringLiteral("Device"), FieldType::Text, false, {}},
            {QStringLiteral("name"), QStringLiteral("Name"), FieldType::Text, false, {}},
            {QStringLiteral("uniqueId"), QStringLiteral("Unique ID"), FieldType::Text, false, {}},
            {QStringLiteral("barcode"), QStringLiteral("条码"), FieldType::Text, false, {}},
            {QStringLiteral("minStock"), QStringLiteral("最低安全库存"), FieldType::Integer, false, {}},
            {QStringLiteral("batchNo"), QStringLiteral("批次号"), FieldType::Text, false, {}},
            {QStringLiteral("supplier"), QStringLiteral("供应商"), FieldType::Text, false, {}},
            {QStringLiteral("currentRating"), QStringLiteral("Current Rating"), FieldType::Text, false, {}},
            {QStringLiteral("currentRatingMax"), QStringLiteral("Current Rating (Max)"), FieldType::Text, false, {}},
            {QStringLiteral("dcResistanceDcr"), QStringLiteral("DC Resistance(DCR)"), FieldType::Text, false, {}},
            {QStringLiteral("equivalentSeriesResistanceEsr"), QStringLiteral("Equivalent Series Resistance(ESR)"), FieldType::Text, false, {}},
            {QStringLiteral("gateChargeQg"), QStringLiteral("Gate Charge(Qg)"), FieldType::Text, false, {}},
            {QStringLiteral("gateThresholdVoltageVgsTh"), QStringLiteral("Gate Threshold Voltage (Vgs(th))"), FieldType::Text, false, {}},
            {QStringLiteral("overloadVoltageMax"), QStringLiteral("Overload Voltage (Max)"), FieldType::Text, false, {}}
        },
        {
            QStringLiteral("uniqueId"),
            QStringLiteral("barcode"),
            QStringLiteral("name"),
            QStringLiteral("manufacturerPart"),
            QStringLiteral("manufacturer"),
            QStringLiteral("quantity"),
            QStringLiteral("unit"),
            QStringLiteral("location"),
            QStringLiteral("date"),
            QStringLiteral("category"),
            QStringLiteral("device"),
            QStringLiteral("comment"),
            QStringLiteral("designator")
        },
        {
            QStringLiteral("barcode"),
            QStringLiteral("manufacturerPart"),
            QStringLiteral("quantity"),
            QStringLiteral("category"),
            QStringLiteral("date"),
            QStringLiteral("location")
        },
        false
    };
}

PageConfig reimbursementConfig()
{
    return {
        QStringLiteral("reimbursement"),
        QStringLiteral("报账管理"),
        {
            {QStringLiteral("category"), QStringLiteral("报账类别"), FieldType::Text, true, {}},
            {QStringLiteral("reimbursementOwner"), QStringLiteral("报账人"), FieldType::Text, true, {}},
            {QStringLiteral("description"), QStringLiteral("报账说明"), FieldType::Multiline, true, {}},
            {QStringLiteral("amount"), QStringLiteral("金额"), FieldType::Double, true, {}},
            {QStringLiteral("date"), QStringLiteral("日期"), FieldType::Date, true, {}},
            {QStringLiteral("invoiceIssued"), QStringLiteral("是否开票"), FieldType::Boolean, false, {}},
            {QStringLiteral("reimbursed"), QStringLiteral("是否已报账"), FieldType::Boolean, false, {}},
            {QStringLiteral("invoiceAttachment"), QStringLiteral("发票附件（PDF）"), FieldType::File, false, {QStringLiteral("PDF 文件 (*.pdf)")}},
            {QStringLiteral("note"), QStringLiteral("备注"), FieldType::Multiline, false, {}},
            {QStringLiteral("otherAttachments"), QStringLiteral("其它附件"), FieldType::Files, false, {QStringLiteral("附件文件 (*.pdf *.png *.jpg *.jpeg *.bmp *.gif *.webp)")}}
        },
        {
            QStringLiteral("category"),
            QStringLiteral("reimbursementOwner"),
            QStringLiteral("description"),
            QStringLiteral("amount"),
            QStringLiteral("date"),
            QStringLiteral("invoiceIssued"),
            QStringLiteral("reimbursed"),
            QStringLiteral("note")
        },
        {
            QStringLiteral("category"),
            QStringLiteral("reimbursementOwner"),
            QStringLiteral("description"),
            QStringLiteral("amount"),
            QStringLiteral("date"),
            QStringLiteral("invoiceIssued"),
            QStringLiteral("reimbursed"),
            QStringLiteral("note")
        },
        true
    };
}

}

MainWindow::MainWindow(AppService *storageService,
                       const QString &statusMessage,
                 std::function<void(QWidget *)> openConnectionSettings,
                       QWidget *parent)
    : QMainWindow(parent),
      m_storageService(storageService),
    m_statusMessage(statusMessage),
    m_openConnectionSettings(std::move(openConnectionSettings))
{
    applyTheme();
    buildUi();
    resize(1180, 720);
    setWindowTitle(QStringLiteral("管理软件原型"));
    statusBar()->showMessage(m_statusMessage);
}

MainWindow::~MainWindow()
{
    delete m_storageService;
}

void MainWindow::applyTheme()
{
    setStyleSheet(QStringLiteral(
    "QWidget { background-color: #f4f7f8; color: #1d3135; font-family: 'Microsoft YaHei UI'; font-size: 14px; }"
    "QMainWindow, QDialog { background-color: #f4f7f8; }"
    "QFrame { background-color: transparent; border: none; }"
    "QLabel { color: #1d3135; background: transparent; border: none; }"
    "QLineEdit, QTextEdit, QDateEdit, QSpinBox, QDoubleSpinBox, QComboBox, QTableWidget {"
    "  background-color: #fbfdfd;"
    "  color: #1d3135;"
    "  border: 1px solid #d7e3e5;"
    "  border-radius: 14px;"
    "  padding: 9px 12px;"
    "  selection-background-color: #c7dcda;"
    "  selection-color: #163035;"
    "}"
    "QLineEdit:focus, QTextEdit:focus, QDateEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:focus { border-color: #6d9894; }"
    "QTableWidget { alternate-background-color: #f6faf9; outline: none; padding: 0px; gridline-color: #e8eff0; }"
    "QTableWidget::item { padding: 10px 12px; border-bottom: 1px solid #edf3f4; }"
    "QTableWidget::item:selected { background-color: #dcefee; color: #173137; border-top: 1px solid #b7d7d4; border-bottom: 1px solid #b7d7d4; }"
    "QTableWidget::item:selected:active { background-color: #d3e9e7; color: #102b31; }"
    "QTableWidget::item:selected:!active { background-color: #e4f2f0; color: #2d4a50; }"
    "QHeaderView::section { background-color: #f7fbfb; color: #5e7478; border: none; border-bottom: 1px solid #e2ebec; padding: 13px 14px; font-weight: 700; }"
    "QPushButton {"
    "  background-color: #fbfdfd;"
    "  color: #1d3135;"
    "  border: 1px solid #d7e3e5;"
    "  border-radius: 14px;"
    "  padding: 10px 16px;"
    "  font-weight: 600;"
    "}"
    "QPushButton:hover { background-color: #f5fbfb; border-color: #bfd2d4; }"
    "QPushButton:pressed { background-color: #eaf4f3; }"
    "QPushButton[variant='primary'] { background-color: #5f8e8a; color: #ffffff; border: 1px solid #5f8e8a; }"
    "QPushButton[variant='primary']:hover { background-color: #547f7b; border-color: #547f7b; }"
    "QPushButton[variant='primary']:pressed { background-color: #486e6b; }"
    "QPushButton[variant='danger'] { background-color: #fff4f2; color: #b14d43; border: 1px solid #f0c7c1; }"
    "QPushButton[variant='danger']:hover { background-color: #fde8e5; border-color: #e5b4ad; }"
    "QPushButton[variant='subtle'] { background-color: #f7fbfb; color: #557075; border: 1px solid #dce7e8; }"
    "QPushButton[variant='subtle']:hover { background-color: #eef6f6; border-color: #c8d9db; }"
    "QPushButton[variant='tab'] {"
    "  background-color: transparent;"
    "  color: #5d767b;"
    "  border: 1px solid transparent;"
    "  border-radius: 12px;"
    "  padding: 9px 16px;"
    "}"
    "QPushButton[variant='tab']:hover { background-color: #f5fbfb; border-color: #d7e5e6; color: #234147; }"
    "QPushButton[variant='tab'][active='true'] { background-color: #fbfdfd; color: #173137; border-color: #cddedf; }"
        "QPushButton[variant='nav'] {"
        "  text-align: left;"
    "  padding: 15px 18px;"
    "  border-radius: 16px;"
        "  background-color: transparent;"
        "  border: 1px solid transparent;"
    "  color: #61777b;"
        "}"
    "QPushButton[variant='nav']:hover { background-color: #f2f8f8; border-color: #d7e5e6; color: #284248; }"
    "QPushButton[variant='nav'][active='true'] { background-color: #e6f1f0; border-color: #c8dbd9; color: #18333a; }"
    "QToolButton {"
    "  background-color: #f7fbfb;"
    "  color: #557075;"
    "  border: 1px solid #dce7e8;"
    "  border-radius: 14px;"
    "}"
    "QToolButton:hover { background-color: #eef6f6; border-color: #c8d9db; }"
    "QComboBox::drop-down { border: none; width: 28px; }"
    "QComboBox::down-arrow { width: 10px; height: 10px; }"
    "QCheckBox { spacing: 8px; }"
    "QMenu { background-color: #fbfdfd; color: #1d3135; border: 1px solid #d7e3e5; padding: 8px; border-radius: 14px; }"
    "QMenu::item { padding: 8px 18px; border-radius: 8px; }"
    "QMenu::item:selected { background-color: #edf5f5; }"
        "QScrollBar:vertical, QScrollBar:horizontal { background: transparent; border: none; margin: 4px; }"
    "QScrollBar::handle:vertical, QScrollBar::handle:horizontal { background: #c5d7d8; border-radius: 6px; min-height: 28px; min-width: 28px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical, QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0px; height: 0px; }"
    "QStatusBar { background-color: #f7fbfb; color: #70868b; border-top: 1px solid #deeaeb; }"));
}

void MainWindow::buildUi()
{
    auto *centralWidget = new QWidget(this);
    auto *rootLayout = new QHBoxLayout(centralWidget);
    rootLayout->setContentsMargins(20, 20, 20, 14);
    rootLayout->setSpacing(20);

    auto *navigationFrame = new QFrame(centralWidget);
    navigationFrame->setObjectName(QStringLiteral("navigationFrame"));
    navigationFrame->setFixedWidth(260);
    navigationFrame->setStyleSheet(QStringLiteral(
        "#navigationFrame { background-color: #fbfdfd; color: #1d3135; border: 1px solid #dde8e9; border-radius: 28px; }"));
    auto *navigationLayout = new QVBoxLayout(navigationFrame);
    navigationLayout->setContentsMargins(22, 24, 22, 24);
    navigationLayout->setSpacing(14);

    auto *brandLabel = new QLabel(QStringLiteral("管理中心"), navigationFrame);
    brandLabel->setStyleSheet(QStringLiteral("font-size: 28px; font-weight: 800; letter-spacing: 1px; color: #173137;"));
    auto *navigationSectionLabel = new QLabel(QStringLiteral("业务模块"), navigationFrame);
    navigationSectionLabel->setStyleSheet(QStringLiteral("font-size: 12px; font-weight: 700; color: #7a9195; padding-top: 10px;"));
    m_inventoryButton = new QPushButton(QStringLiteral("库存管理"), navigationFrame);
    m_reimbursementButton = new QPushButton(QStringLiteral("报账管理"), navigationFrame);
    m_inventoryButton->setProperty("variant", QStringLiteral("nav"));
    m_reimbursementButton->setProperty("variant", QStringLiteral("nav"));
    m_inventoryButton->setCheckable(true);
    m_reimbursementButton->setCheckable(true);
    m_inventoryButton->setMinimumHeight(54);
    m_reimbursementButton->setMinimumHeight(54);

    navigationLayout->addWidget(brandLabel);
    navigationLayout->addSpacing(18);
    navigationLayout->addWidget(navigationSectionLabel);
    navigationLayout->addWidget(m_inventoryButton);
    navigationLayout->addWidget(m_reimbursementButton);
    navigationLayout->addStretch();

    auto *contentWidget = new QWidget(centralWidget);
    auto *contentLayout = new QVBoxLayout(contentWidget);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(16);

    auto *headerFrame = new QFrame(contentWidget);
    headerFrame->setObjectName(QStringLiteral("headerFrame"));
    headerFrame->setStyleSheet(QStringLiteral(
        "#headerFrame { background-color: #fbfdfd; border: 1px solid #dde8e9; border-radius: 28px; }"));
    auto *headerLayout = new QHBoxLayout(headerFrame);
    headerLayout->setContentsMargins(28, 22, 28, 22);
    headerLayout->setSpacing(18);

    auto *headerTextLayout = new QVBoxLayout();
    headerTextLayout->setSpacing(6);
    m_pageTitleLabel = new QLabel(headerFrame);
    m_pageTitleLabel->setStyleSheet(QStringLiteral("font-size: 28px; font-weight: 800; color: #173137;"));
    headerTextLayout->addWidget(m_pageTitleLabel);

    m_reimbursementTabsWidget = new QFrame(headerFrame);
    m_reimbursementTabsWidget->setStyleSheet(QStringLiteral(
        "background-color: #f2f8f8; border: 1px solid #d8e6e7; border-radius: 16px;"));
    auto *reimbursementTabsLayout = new QHBoxLayout(m_reimbursementTabsWidget);
    reimbursementTabsLayout->setContentsMargins(6, 6, 6, 6);
    reimbursementTabsLayout->setSpacing(6);

    m_reimbursementOverviewTabButton = new QPushButton(QStringLiteral("报账总览"), m_reimbursementTabsWidget);
    m_reimbursementDetailTabButton = new QPushButton(QStringLiteral("报账明细"), m_reimbursementTabsWidget);
    m_reimbursementOverviewTabButton->setProperty("variant", QStringLiteral("tab"));
    m_reimbursementDetailTabButton->setProperty("variant", QStringLiteral("tab"));
    m_reimbursementOverviewTabButton->setCheckable(true);
    m_reimbursementDetailTabButton->setCheckable(true);
    reimbursementTabsLayout->addWidget(m_reimbursementOverviewTabButton);
    reimbursementTabsLayout->addWidget(m_reimbursementDetailTabButton);

    auto *dateLabel = new QLabel(QDate::currentDate().toString(QStringLiteral("yyyy.MM.dd")), headerFrame);
    dateLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    dateLabel->setStyleSheet(QStringLiteral(
        "font-size: 13px; color: #587277; font-weight: 700;"
        "background-color: #f2f8f8; border: 1px solid #d8e6e7; border-radius: 14px; padding: 10px 14px;"));

    auto *settingsButton = new QPushButton(QStringLiteral("连接设置"), headerFrame);
    settingsButton->setMinimumHeight(40);
    settingsButton->setProperty("variant", QStringLiteral("primary"));
    settingsButton->setStyleSheet(QStringLiteral("font-size: 13px; padding: 8px 16px;"));
    connect(settingsButton, &QPushButton::clicked, this, [this]() {
        if (m_openConnectionSettings) {
            m_openConnectionSettings(this);
        }
    });

    auto *accountSecurityButton = new QPushButton(QStringLiteral("账户安全"), headerFrame);
    accountSecurityButton->setMinimumHeight(40);
    accountSecurityButton->setProperty("variant", QStringLiteral("subtle"));
    accountSecurityButton->setStyleSheet(QStringLiteral("font-size: 13px; padding: 8px 16px;"));
    connect(accountSecurityButton, &QPushButton::clicked, this, [this]() {
        AccountSecurityDialog dialog(m_storageService, this);
        dialog.exec();
    });

    headerLayout->addLayout(headerTextLayout, 1);
    headerLayout->addWidget(m_reimbursementTabsWidget);
    headerLayout->addWidget(accountSecurityButton);
    headerLayout->addWidget(settingsButton);
    headerLayout->addWidget(dateLabel);

    m_stack = new QStackedWidget(contentWidget);
    m_stack->setStyleSheet(QStringLiteral("QStackedWidget { background-color: transparent; }"));
    m_inventoryPage = new ManagementPage(inventoryConfig(), m_storageService, m_stack);
    m_reimbursementOverviewPage = new ReimbursementOverviewPage(m_storageService, m_stack);
    m_reimbursementDetailPage = new ManagementPage(reimbursementConfig(), m_storageService, m_stack);
    m_stack->addWidget(m_inventoryPage);
    m_stack->addWidget(m_reimbursementOverviewPage);
    m_stack->addWidget(m_reimbursementDetailPage);

    connect(m_inventoryButton, &QPushButton::clicked, this, [this]() { setCurrentPage(0); });
    connect(m_reimbursementButton, &QPushButton::clicked, this, [this]() { setCurrentPage(1); });
    connect(m_reimbursementOverviewTabButton, &QPushButton::clicked, this, [this]() { setCurrentPage(1); });
    connect(m_reimbursementDetailTabButton, &QPushButton::clicked, this, [this]() { setCurrentPage(2); });

    contentLayout->addWidget(headerFrame);
    contentLayout->addWidget(m_stack, 1);

    rootLayout->addWidget(navigationFrame);
    rootLayout->addWidget(contentWidget, 1);
    setCentralWidget(centralWidget);
    setCurrentPage(0);
}

void MainWindow::setCurrentPage(int index)
{
    m_stack->setCurrentIndex(index);

    if (index == 0 && m_inventoryPage != nullptr) {
        m_inventoryPage->reloadRecords();
    } else if (index == 1 && m_reimbursementOverviewPage != nullptr) {
        m_reimbursementOverviewPage->reloadData();
    } else if (index == 2 && m_reimbursementDetailPage != nullptr) {
        m_reimbursementDetailPage->reloadRecords();
    }

    updateNavigationState();

    if (index == 0) {
        m_pageTitleLabel->setText(QStringLiteral("库存管理"));
        return;
    }

    if (index == 1) {
        m_pageTitleLabel->setText(QStringLiteral("报账管理"));
        return;
    }

    m_pageTitleLabel->setText(QStringLiteral("报账管理"));
}

void MainWindow::updateNavigationState()
{
    const int currentIndex = m_stack->currentIndex();
    const bool inventoryActive = currentIndex == 0;
    const bool reimbursementActive = currentIndex == 1 || currentIndex == 2;
    const bool reimbursementOverviewActive = currentIndex == 1;
    const bool reimbursementDetailActive = currentIndex == 2;
    m_inventoryButton->setChecked(inventoryActive);
    m_reimbursementButton->setChecked(reimbursementActive);
    m_reimbursementOverviewTabButton->setChecked(reimbursementOverviewActive);
    m_reimbursementDetailTabButton->setChecked(reimbursementDetailActive);
    m_inventoryButton->setProperty("active", inventoryActive);
    m_reimbursementButton->setProperty("active", reimbursementActive);
    m_reimbursementOverviewTabButton->setProperty("active", reimbursementOverviewActive);
    m_reimbursementDetailTabButton->setProperty("active", reimbursementDetailActive);
    m_reimbursementTabsWidget->setVisible(reimbursementActive);

    for (QPushButton *button : {m_inventoryButton, m_reimbursementButton, m_reimbursementOverviewTabButton, m_reimbursementDetailTabButton}) {
        style()->unpolish(button);
        style()->polish(button);
        button->update();
    }
}
