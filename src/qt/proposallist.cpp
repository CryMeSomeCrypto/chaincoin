// Copyright (c) 2011-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "proposallist.h"
#include "guiutil.h"
#include "platformstyle.h"
#include "optionsmodel.h"
#include "proposalfilterproxy.h"
#include "proposalrecord.h"
#include "proposaltablemodel.h"
#include "masternodeman.h"
#include "masternodeconfig.h"
#include "masternode.h"
#include "messagesigner.h"
#include "util.h"
#include "governance.h"
 
#include "ui_interface.h"

#include <QComboBox>
#include <QDateTimeEdit>
#include <QDesktopServices>
#include <QDoubleValidator>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QMenu>
#include <QPoint>
#include <QScrollBar>
#include <QSettings>
#include <QTableView>
#include <QUrl>
#include <QVBoxLayout>

/** Date format for persistence */
static const char* PERSISTENCE_DATE_FORMAT = "yyyy-MM-dd";

ProposalList::ProposalList(const PlatformStyle *platformStyle, QWidget *parent) :
    QWidget(parent), proposalTableModel(0), proposalProxyModel(0),
    proposalList(0), columnResizingFixer(0)
{
    proposalTableModel = new ProposalTableModel(platformStyle, this); 
    QSettings settings;

    setContentsMargins(0,0,0,0);

    hlayout = new ColumnAlignedLayout();
    hlayout->setContentsMargins(0,0,0,0);
    hlayout->setSpacing(0);

    proposalWidget = new QLineEdit(this);
#if QT_VERSION >= 0x040700
    proposalWidget->setPlaceholderText(tr("Enter proposal name"));
#endif
    proposalWidget->setObjectName("proposalWidget");
    hlayout->addWidget(proposalWidget);

    amountWidget = new QLineEdit(this);
#if QT_VERSION >= 0x040700
    amountWidget->setPlaceholderText(tr("Min amount"));
#endif
    amountWidget->setValidator(new QDoubleValidator(0, 1e20, 8, this));
    amountWidget->setObjectName("amountWidget");
    hlayout->addWidget(amountWidget);

    startDateWidget = new QComboBox(this);
    startDateWidget->addItem(tr("All"), All);
    startDateWidget->addItem(tr("Today"), Today);
    startDateWidget->addItem(tr("This week"), ThisWeek);
    startDateWidget->addItem(tr("This month"), ThisMonth);
    startDateWidget->addItem(tr("Last month"), LastMonth);
    startDateWidget->addItem(tr("This year"), ThisYear);
    startDateWidget->addItem(tr("Range..."), Range);
    startDateWidget->setCurrentIndex(settings.value("proposalStartDateIndex").toInt());
    hlayout->addWidget(startDateWidget);

    endDateWidget = new QComboBox(this);
    endDateWidget->addItem(tr("All"), All);
    endDateWidget->addItem(tr("Today"), Today);
    endDateWidget->addItem(tr("This week"), ThisWeek);
    endDateWidget->addItem(tr("This month"), ThisMonth);
    endDateWidget->addItem(tr("Last month"), LastMonth);
    endDateWidget->addItem(tr("This year"), ThisYear);
    endDateWidget->addItem(tr("Range..."), Range);
    endDateWidget->setCurrentIndex(settings.value("proposalEndDateIndex").toInt());
    hlayout->addWidget(endDateWidget);

    yesVotesWidget = new QLineEdit(this);
#if QT_VERSION >= 0x040700
    yesVotesWidget->setPlaceholderText(tr("Min yes votes"));
#endif
    yesVotesWidget->setValidator(new QIntValidator(0, INT_MAX, this));
    yesVotesWidget->setObjectName("yesVotesWidget");
    hlayout->addWidget(yesVotesWidget);

    noVotesWidget = new QLineEdit(this);
#if QT_VERSION >= 0x040700
    noVotesWidget->setPlaceholderText(tr("Min no votes"));
#endif
    noVotesWidget->setValidator(new QIntValidator(0, INT_MAX, this));
    noVotesWidget->setObjectName("noVotesWidget");
    hlayout->addWidget(noVotesWidget);


    absoluteYesVotesWidget = new QLineEdit(this);
#if QT_VERSION >= 0x040700
    absoluteYesVotesWidget->setPlaceholderText(tr("Min abs. yes votes"));
#endif
    absoluteYesVotesWidget->setValidator(new QIntValidator(INT_MIN, INT_MAX, this));
    absoluteYesVotesWidget->setObjectName("absoluteYesVotesWidget");
    hlayout->addWidget(absoluteYesVotesWidget);

    percentageWidget = new QLineEdit(this);
#if QT_VERSION >= 0x040700
    percentageWidget->setPlaceholderText(tr("Min percentage"));
#endif
    percentageWidget->setValidator(new QIntValidator(-100, 100, this));
    percentageWidget->setObjectName("percentageWidget");
    hlayout->addWidget(percentageWidget);

    QVBoxLayout *vlayout = new QVBoxLayout(this);
    vlayout->setSpacing(0);

    QTableView *view = new QTableView(this);
    vlayout->addLayout(hlayout);
    vlayout->addWidget(createStartDateRangeWidget());
    vlayout->addWidget(createEndDateRangeWidget());
    vlayout->addWidget(view);
    vlayout->setSpacing(0);
    int width = view->verticalScrollBar()->sizeHint().width();
    hlayout->addSpacing(width);
    hlayout->setTableColumnsToTrack(view->horizontalHeader());

    connect(view->horizontalHeader(), SIGNAL(sectionResized(int,int,int)), SLOT(invalidateAlignedLayout()));
    connect(view->horizontalScrollBar(), SIGNAL(valueChanged(int)), SLOT(invalidateAlignedLayout()));

    view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    view->setTabKeyNavigation(false);
    view->setContextMenuPolicy(Qt::CustomContextMenu);

    proposalList = view;

    QHBoxLayout *actionBar = new QHBoxLayout();
    actionBar->setSpacing(11);
    actionBar->setContentsMargins(0,20,0,20);

    QPushButton *voteYesButton = new QPushButton(tr("Vote Yes"), this);
    voteYesButton->setToolTip(tr("Vote Yes on the selected proposal"));
    actionBar->addWidget(voteYesButton);

    QPushButton *voteAbstainButton = new QPushButton(tr("Vote Abstain"), this);
    voteAbstainButton->setToolTip(tr("Vote Abstain on the selected proposal"));
    actionBar->addWidget(voteAbstainButton);

    QPushButton *voteNoButton = new QPushButton(tr("Vote No"), this);
    voteNoButton->setToolTip(tr("Vote No on the selected proposal"));
    actionBar->addWidget(voteNoButton);

    secondsLabel = new QLabel();
    actionBar->addWidget(secondsLabel);
    actionBar->addStretch();

    vlayout->addLayout(actionBar);

    QAction *voteYesAction = new QAction(tr("Vote yes"), this);
    QAction *voteAbstainAction = new QAction(tr("Vote abstain"), this);
    QAction *voteNoAction = new QAction(tr("Vote no"), this);
    QAction *openUrlAction = new QAction(tr("Visit proposal website"), this);
    QAction *openStatisticsAction = new QAction(tr("Visit statistics website"), this);

    contextMenu = new QMenu(this);
    contextMenu->addAction(voteYesAction);
    contextMenu->addAction(voteAbstainAction);
    contextMenu->addAction(voteNoAction);
    contextMenu->addSeparator();
    contextMenu->addAction(openUrlAction);

    connect(voteYesButton, SIGNAL(clicked()), this, SLOT(voteYes()));
    connect(voteAbstainButton, SIGNAL(clicked()), this, SLOT(voteAbstain()));
    connect(voteNoButton, SIGNAL(clicked()), this, SLOT(voteNo()));

    connect(proposalWidget, SIGNAL(textChanged(QString)), this, SLOT(changedProposal(QString)));
    connect(startDateWidget, SIGNAL(activated(int)), this, SLOT(chooseStartDate(int)));
    connect(endDateWidget, SIGNAL(activated(int)), this, SLOT(chooseEndDate(int)));
    connect(yesVotesWidget, SIGNAL(textChanged(QString)), this, SLOT(changedYesVotes(QString)));
    connect(noVotesWidget, SIGNAL(textChanged(QString)), this, SLOT(changedNoVotes(QString)));
    connect(absoluteYesVotesWidget, SIGNAL(textChanged(QString)), this, SLOT(changedAbsoluteYesVotes(QString)));
    connect(yesVotesWidget, SIGNAL(textChanged(QString)), this, SLOT(changedYesVotes(QString)));
    connect(amountWidget, SIGNAL(textChanged(QString)), this, SLOT(changedAmount(QString)));
    connect(percentageWidget, SIGNAL(textChanged(QString)), this, SLOT(changedPercentage(QString)));

    connect(view, SIGNAL(doubleClicked(QModelIndex)), this, SLOT(openProposalUrl()));
    connect(view, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(contextualMenu(QPoint)));

    connect(voteYesAction, SIGNAL(triggered()), this, SLOT(voteYes()));
    connect(voteNoAction, SIGNAL(triggered()), this, SLOT(voteNo()));
    connect(voteAbstainAction, SIGNAL(triggered()), this, SLOT(voteAbstain()));

    connect(openUrlAction, SIGNAL(triggered()), this, SLOT(openProposalUrl()));

    proposalProxyModel = new ProposalFilterProxy(this);
    proposalProxyModel->setSourceModel(proposalTableModel);
    proposalProxyModel->setDynamicSortFilter(true);
    proposalProxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);
    proposalProxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);

    proposalProxyModel->setSortRole(Qt::EditRole);

    proposalList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    proposalList->setModel(proposalProxyModel);
    proposalList->setAlternatingRowColors(true);
    proposalList->setSelectionBehavior(QAbstractItemView::SelectRows);
    proposalList->setSortingEnabled(true);
    proposalList->sortByColumn(ProposalTableModel::StartDate, Qt::DescendingOrder);
    proposalList->verticalHeader()->hide();

    proposalList->setColumnWidth(ProposalTableModel::Proposal, PROPOSAL_COLUMN_WIDTH);
    proposalList->setColumnWidth(ProposalTableModel::StartDate, START_DATE_COLUMN_WIDTH);
    proposalList->setColumnWidth(ProposalTableModel::EndDate, END_DATE_COLUMN_WIDTH);
    proposalList->setColumnWidth(ProposalTableModel::YesVotes, YES_VOTES_COLUMN_WIDTH);
    proposalList->setColumnWidth(ProposalTableModel::NoVotes, NO_VOTES_COLUMN_WIDTH);
    proposalList->setColumnWidth(ProposalTableModel::AbsoluteYesVotes, ABSOLUTE_YES_COLUMN_WIDTH);
    proposalList->setColumnWidth(ProposalTableModel::Amount, AMOUNT_COLUMN_WIDTH);
    proposalList->setColumnWidth(ProposalTableModel::Percentage, PERCENTAGE_COLUMN_WIDTH);

    connect(proposalList->selectionModel(), SIGNAL(selectionChanged(QItemSelection, QItemSelection)), this, SLOT(computeSum()));

    columnResizingFixer = new GUIUtil::TableViewLastColumnResizingFixer(proposalList, PERCENTAGE_COLUMN_WIDTH, MINIMUM_COLUMN_WIDTH, this);
        
    chooseStartDate(settings.value("proposalStartDate").toInt());
    chooseEndDate(settings.value("proposalEndDate").toInt());

    nLastUpdate = GetTime();

    timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(refreshProposals()));
    timer->start(1000);

    setLayout(vlayout);
}

void ProposalList::invalidateAlignedLayout() {
    hlayout->invalidate();
}

void ProposalList::refreshProposals(bool force) {
    int64_t secondsRemaining = nLastUpdate - GetTime() + PROPOSALLIST_UPDATE_SECONDS;

    QString secOrMinutes = (secondsRemaining / 60 > 1) ? tr("minute(s)") : tr("second(s)");
    secondsLabel->setText(tr("List will be updated in %1 %2").arg((secondsRemaining > 60) ? QString::number(secondsRemaining / 60) : QString::number(secondsRemaining), secOrMinutes));

    if(secondsRemaining > 0 && !force) return;
    nLastUpdate = GetTime();

    proposalTableModel->refreshProposals();

    secondsLabel->setText(tr("List will be updated in 0 second(s)"));
}

void ProposalList::chooseStartDate(int idx)
{
    if(!proposalProxyModel)
        return;
    
    QSettings settings;
    QDate current = QDate::currentDate();
    startDateRangeWidget->setVisible(false);
    switch(startDateWidget->itemData(idx).toInt())
    {
    case All:
        proposalProxyModel->setProposalStart(
                ProposalFilterProxy::MIN_DATE);
        break;
    case Today:
        proposalProxyModel->setProposalStart(
                QDateTime(current));
        break;
    case ThisWeek: {
        QDate startOfWeek = current.addDays(-(current.dayOfWeek()-1));
        proposalProxyModel->setProposalStart(
                QDateTime(startOfWeek));

        } break;
    case ThisMonth:
        proposalProxyModel->setProposalStart(
                QDateTime(QDate(current.year(), current.month(), 1)));
        break;
    case LastMonth:
        proposalProxyModel->setProposalStart(
                QDateTime(QDate(current.year(), current.month(), 1).addMonths(-1)));
        break;
    case ThisYear:
        proposalProxyModel->setProposalStart(
                QDateTime(QDate(current.year(), 1, 1)));
        break;
    case Range:
        startDateRangeWidget->setVisible(true);
        startDateRangeChanged();
        break;
    }
    
    settings.setValue("proposalStartDateIndex", idx);
    if (startDateWidget->itemData(idx).toInt() == Range)
        settings.setValue("proposalStartDate", proposalStartDate->date().toString(PERSISTENCE_DATE_FORMAT));
}

void ProposalList::chooseEndDate(int idx)
{
    if(!proposalProxyModel)
        return;
    
    QSettings settings;
    QDate current = QDate::currentDate();
    endDateRangeWidget->setVisible(false);
    switch(endDateWidget->itemData(idx).toInt())
    {
    case All:
        proposalProxyModel->setProposalEnd(
                ProposalFilterProxy::MAX_DATE);
        break;
    case Today:
        proposalProxyModel->setProposalEnd(
                QDateTime(current));
        break;
    case ThisWeek: {
        QDate startOfWeek = current.addDays(-(current.dayOfWeek()-1));
        proposalProxyModel->setProposalEnd(
                QDateTime(startOfWeek));

        } break;
    case ThisMonth:
        proposalProxyModel->setProposalEnd(
                QDateTime(QDate(current.year(), current.month(), 1)));
        break;
    case LastMonth:
        proposalProxyModel->setProposalEnd(
                QDateTime(QDate(current.year(), current.month(), 1).addMonths(-1)));
        break;
    case ThisYear:
        proposalProxyModel->setProposalEnd(
                QDateTime(QDate(current.year(), 1, 1)));
        break;
    case Range:
        endDateRangeWidget->setVisible(true);
        endDateRangeChanged();
        break;
    }
    
    settings.setValue("proposalEndDateIndex", idx);
    if (endDateWidget->itemData(idx).toInt() == Range)
        settings.setValue("proposalEndDate", proposalEndDate->date().toString(PERSISTENCE_DATE_FORMAT));
}


void ProposalList::changedAmount(const QString &minAmount)
{
    if(!proposalProxyModel)
        return;

    proposalProxyModel->setMinAmount(minAmount.toInt());
}

void ProposalList::changedPercentage(const QString &minPercentage)
{
    if(!proposalProxyModel)
        return;

    int value = minPercentage == "" ? -100 : minPercentage.toInt();

    proposalProxyModel->setMinPercentage(value);
}

void ProposalList::changedProposal(const QString &proposal)
{
    if(!proposalProxyModel)
        return;

    proposalProxyModel->setProposal(proposal);
}

void ProposalList::changedYesVotes(const QString &minYesVotes)
{
    if(!proposalProxyModel)
        return;

    proposalProxyModel->setMinYesVotes(minYesVotes.toInt());
}

void ProposalList::changedNoVotes(const QString &minNoVotes)
{
    if(!proposalProxyModel)
        return;

    proposalProxyModel->setMinNoVotes(minNoVotes.toInt());
}

void ProposalList::changedAbsoluteYesVotes(const QString &minAbsoluteYesVotes)
{
    if(!proposalProxyModel)
        return;

    int value = minAbsoluteYesVotes == "" ? INT_MIN : minAbsoluteYesVotes.toInt();

    proposalProxyModel->setMinAbsoluteYesVotes(value);
}

void ProposalList::contextualMenu(const QPoint &point)
{
    QModelIndex index = proposalList->indexAt(point);
    QModelIndexList selection = proposalList->selectionModel()->selectedRows(0);
    if (selection.empty())
        return;

    if(index.isValid())
        contextMenu->exec(QCursor::pos());
}

void ProposalList::voteYes()
{
    vote_click_handler("yes");
}

void ProposalList::voteNo()
{
    vote_click_handler("no");
}

void ProposalList::voteAbstain()
{
    vote_click_handler("abstain");
}

void ProposalList::vote_click_handler(const std::string voteString)
{
    if(!proposalList->selectionModel())
        return;

    QModelIndexList selection = proposalList->selectionModel()->selectedRows();
    if(selection.empty())
        return;

    QString proposalName = selection.at(0).data(ProposalTableModel::ProposalRole).toString();

    QMessageBox::StandardButton retval = QMessageBox::question(this, tr("Confirm vote"),
        tr("Are you sure you want to vote <strong>%1</strong> on the proposal <strong>%2</strong>?").arg(QString::fromStdString(voteString), proposalName),
        QMessageBox::Yes | QMessageBox::Cancel,
        QMessageBox::Cancel);

    if(retval != QMessageBox::Yes) return;

    uint256 hash;
    hash.SetHex(selection.at(0).data(ProposalTableModel::ProposalHashRole).toString().toStdString());

    vote_signal_enum_t eVoteSignal = CGovernanceVoting::ConvertVoteSignal("funding");
    vote_outcome_enum_t eVoteOutcome = CGovernanceVoting::ConvertVoteOutcome(voteString);

    int nSuccessful = 0;
    int nFailed = 0;

    for (const auto& mne : masternodeConfig.getEntries()) {
        std::string strError;
        std::vector<unsigned char> vchMasterNodeSignature;
        std::string strMasterNodeSignMessage;

        CPubKey pubKeyCollateralAddress;
        CKey keyCollateralAddress;
        CPubKey pubKeyMasternode;
        CKey keyMasternode;

        UniValue statusObj(UniValue::VOBJ);

        if(!CMessageSigner::GetKeysFromSecret(mne.getPrivKey(), keyMasternode, pubKeyMasternode)){
            nFailed++;
            continue;
         }

        uint256 nTxHash;
        nTxHash.SetHex(mne.getTxHash());

        int nOutputIndex = 0;
        if(!ParseInt32(mne.getOutputIndex(), &nOutputIndex)) {
            continue;
        }

        COutPoint outpoint(nTxHash, nOutputIndex);

        CMasternode mn;
        bool fMnFound = mnodeman.Get(outpoint, mn);

        if(!fMnFound) {
            nFailed++;
            continue;
        }

        CGovernanceVote vote(mn.outpoint, hash, eVoteSignal, eVoteOutcome);
        if(!vote.Sign(keyMasternode, pubKeyMasternode)){
            nFailed++;
            continue;
        }

        CGovernanceException exception;
        if(governance.ProcessVoteAndRelay(vote, exception, g_connman.get())) {
            nSuccessful++;
        }
        else {
            nFailed++;
        }
    }

    QMessageBox::information(this, tr("Voting"),
        tr("You voted %1 %2 time(s) successfully and failed %3 time(s) on %4").arg(QString::fromStdString(voteString), QString::number(nSuccessful), QString::number(nFailed), proposalName));

    refreshProposals(true);
}

void ProposalList::openProposalUrl()
{
    if(!proposalList || !proposalList->selectionModel())
        return;

    QModelIndexList selection = proposalList->selectionModel()->selectedRows(0);
    if(!selection.isEmpty())
         QDesktopServices::openUrl(selection.at(0).data(ProposalTableModel::ProposalUrlRole).toString());
}

QWidget *ProposalList::createStartDateRangeWidget()
{
    QString defaultDate = QDate::currentDate().toString(PERSISTENCE_DATE_FORMAT);
    QSettings settings;
 
    startDateRangeWidget = new QFrame();
    startDateRangeWidget->setFrameStyle(QFrame::Panel | QFrame::Raised);
    startDateRangeWidget->setContentsMargins(1,1,1,1);
    QHBoxLayout *layout = new QHBoxLayout(startDateRangeWidget);
    layout->setContentsMargins(0,0,0,0);
    layout->addSpacing(23);
    layout->addWidget(new QLabel(tr("Start Date:")));

    proposalStartDate = new QDateTimeEdit(this);
    proposalStartDate->setCalendarPopup(true);
    proposalStartDate->setMinimumWidth(100);

    proposalStartDate->setDate(QDate::fromString(settings.value("proposalStartDate", defaultDate).toString(), PERSISTENCE_DATE_FORMAT));

    layout->addWidget(proposalStartDate);
    layout->addStretch();

    startDateRangeWidget->setVisible(false);

    connect(proposalStartDate, SIGNAL(dateChanged(QDate)), this, SLOT(startDateRangeChanged()));

    return startDateRangeWidget;
}

QWidget *ProposalList::createEndDateRangeWidget()
{
    QString defaultDate = QDate::currentDate().toString(PERSISTENCE_DATE_FORMAT);
    QSettings settings;
 
    endDateRangeWidget = new QFrame();
    endDateRangeWidget->setFrameStyle(QFrame::Panel | QFrame::Raised);
    endDateRangeWidget->setContentsMargins(1,1,1,1);
    QHBoxLayout *layout = new QHBoxLayout(endDateRangeWidget);
    layout->setContentsMargins(0,0,0,0);
    layout->addSpacing(23);
    layout->addWidget(new QLabel(tr("End Date:")));

    proposalEndDate = new QDateTimeEdit(this);
    proposalEndDate->setCalendarPopup(true);
    proposalEndDate->setMinimumWidth(100);

    proposalEndDate->setDate(QDate::fromString(settings.value("proposlEndDate", defaultDate).toString(), PERSISTENCE_DATE_FORMAT));

    layout->addWidget(proposalEndDate);
    layout->addStretch();

    endDateRangeWidget->setVisible(false);

    connect(proposalEndDate, SIGNAL(dateChanged(QDate)), this, SLOT(endDateRangeChanged()));

    return endDateRangeWidget;
}

void ProposalList::startDateRangeChanged()
{
    if(!proposalProxyModel)
        return;
    
    QSettings settings;
    settings.setValue("proposalStartDate", proposalStartDate->date().toString(PERSISTENCE_DATE_FORMAT));
    
    proposalProxyModel->setProposalStart(
            QDateTime(proposalStartDate->date()));
}

void ProposalList::endDateRangeChanged()
{
    if(!proposalProxyModel)
        return;
    
    QSettings settings;
    settings.setValue("proposalEndDate", proposalEndDate->date().toString(PERSISTENCE_DATE_FORMAT));
    
    proposalProxyModel->setProposalEnd(
            QDateTime(proposalEndDate->date()));
}

void ProposalList::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    columnResizingFixer->stretchColumnWidth(ProposalTableModel::Proposal);
}
