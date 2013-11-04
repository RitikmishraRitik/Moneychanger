
#include <QDebug>
#include <QFile>
#include <QFileDialog>
#include <QMessageBox>

#include "assetdetails.h"
#include "ui_assetdetails.h"

#include "wizardaddcontract.h"

#include "moneychanger.h"

#include "detailedit.h"

#include <opentxs/OTAPI.h>
#include <opentxs/OT_ME.h>

MTAssetDetails::MTAssetDetails(QWidget *parent, MTDetailEdit & theOwner) :
    MTEditDetails(parent, theOwner),
    m_pDownloader(NULL),
    ui(new Ui::MTAssetDetails)
{
    ui->setupUi(this);
    this->setContentsMargins(0, 0, 0, 0);
//  this->installEventFilter(this); // NOTE: Successfully tested theory that the base class has already installed this.

    ui->lineEditID->setStyleSheet("QLineEdit { background-color: lightgray }");
}

MTAssetDetails::~MTAssetDetails()
{
    delete ui;
}



void MTAssetDetails::FavorLeftSideForIDs()
{
    if (NULL != ui)
    {
        ui->lineEditID  ->home(false);
        ui->lineEditName->home(false);
    }
}

void MTAssetDetails::ClearContents()
{
    ui->lineEditID  ->setText("");
    ui->lineEditName->setText("");
}

// ------------------------------------------------------

bool MTAssetDetails::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::Resize)
    {
        // This insures that the left-most part of the IDs and Names
        // remains visible during all resize events.
        //
        FavorLeftSideForIDs();
    }
//    else
//    {
        // standard event processing
//        return QObject::eventFilter(obj, event);
        return MTEditDetails::eventFilter(obj, event);

        // NOTE: Since the base class has definitely already installed this
        // function as the event filter, I must assume that this version
        // is overriding the version in the base class.
        //
        // Therefore I call the base class version here, since as it's overridden,
        // I don't expect it will otherwise ever get called.
//    }
}


// ------------------------------------------------------


//virtual
void MTAssetDetails::DeleteButtonClicked()
{
    if (!m_pOwner->m_qstrCurrentID.isEmpty())
    {
        // ----------------------------------------------------
        bool bCanRemove = OTAPI_Wrap::Wallet_CanRemoveAssetType(m_pOwner->m_qstrCurrentID.toStdString());

        if (!bCanRemove)
        {
            QMessageBox::warning(this, tr("Asset Contract Cannot Be Removed"),
                                 tr("This Asset Contract cannot be removed, since you probably have already created accounts "
                                         "using this asset type. (This is where, in the future, you would be given the option "
                                         "to automatically delete those accounts, and remove this asset contract along with them.)"));
            return;
        }
        // ----------------------------------------------------
        QMessageBox::StandardButton reply;

        reply = QMessageBox::question(this, "", tr("Are you sure you want to delete this Asset Contract?"),
                                      QMessageBox::Yes|QMessageBox::No);
        if (reply == QMessageBox::Yes)
        {
            bool bSuccess = OTAPI_Wrap::Wallet_RemoveAssetType(m_pOwner->m_qstrCurrentID.toStdString());

            if (bSuccess)
            {
                m_pOwner->m_map.remove(m_pOwner->m_qstrCurrentID);
                m_pOwner->RefreshRecords();
                // ------------------------------------------------
                if (NULL != m_pMoneychanger)
                    m_pMoneychanger->SetupMainMenu();
                // ------------------------------------------------
            }
            else
                QMessageBox::warning(this, tr("Failure Removing Asset Contract"),
                                     tr("Failed trying to remove this Asset Contract."));
        }
    }
    // ----------------------------------------------------
}


// ------------------------------------------------------

void MTAssetDetails::DownloadedURL()
{
    QString qstrContents(m_pDownloader->downloadedData());
    // ----------------------------
    if (qstrContents.isEmpty())
    {
        QMessageBox::warning(this, tr("File at URL Was Empty"),
                             tr("File at specified URL was apparently empty"));
        return;
    }
    // ----------------------------
    ImportContract(qstrContents);
}

// ------------------------------------------------------

void MTAssetDetails::ImportContract(QString qstrContents)
{
    if (qstrContents.isEmpty())
    {
        QMessageBox::warning(this, tr("Contract is Empty"),
            tr("Failed importing: contract is empty."));
        return;
    }
    // ------------------------------------------------------
    QString qstrContractID = QString::fromStdString(OTAPI_Wrap::CalculateAssetContractID(qstrContents.toStdString()));

    if (qstrContractID.isEmpty())
    {
        QMessageBox::warning(this, tr("Failed Calculating Contract ID"),
                             tr("Failed trying to calculate this contract's ID. Perhaps the 'contract' is malformed?"));
        return;
    }
    // ------------------------------------------------------
    else
    {
        // Already in the wallet?
        //
//        std::string str_Contract = OTAPI_Wrap::LoadAssetContract(qstrContractID.toStdString());
//
//        if (!str_Contract.empty())
//        {
//            QMessageBox::warning(this, tr("Contract Already in Wallet"),
//                tr("Failed importing this contract, since it's already in the wallet."));
//            return;
//        }
        // ---------------------------------------------------
        int32_t nAdded = OTAPI_Wrap::AddAssetContract(qstrContents.toStdString());

        if (1 != nAdded)
        {
            QMessageBox::warning(this, tr("Failed Importing Asset Contract"),
                tr("Failed trying to import contract. Is it already in the wallet?"));
            return;
        }
        // -----------------------------------------------
        QString qstrContractName = QString::fromStdString(OTAPI_Wrap::GetAssetType_Name(qstrContractID.toStdString()));
        // -----------------------------------------------
        QMessageBox::information(this, tr("Success!"), QString("%1: '%2' %3: %4").arg(tr("Success Importing Asset Contract! Name")).
                                 arg(qstrContractName).arg(tr("ID")).arg(qstrContractID));
        // ----------
        m_pOwner->m_map.insert(qstrContractID, qstrContractName);
        m_pOwner->SetPreSelected(qstrContractID);
        m_pOwner->RefreshRecords();
        // ------------------------------------------------
        if (NULL != m_pMoneychanger)
            m_pMoneychanger->SetupMainMenu();
        // ------------------------------------------------
    } // if (!qstrContractID.isEmpty())
}

// ------------------------------------------------------

//virtual
void MTAssetDetails::AddButtonClicked()
{
    MTWizardAddContract theWizard(this);

    theWizard.setWindowTitle(tr("Add Asset Contract"));

    QString qstrDefaultValue("https://raw.github.com/FellowTraveler/Open-Transactions/master/sample-data/sample-contracts/btc.otc");
    QVariant varDefault(qstrDefaultValue);

    theWizard.setField(QString("URL"), varDefault);

    if (QDialog::Accepted == theWizard.exec())
    {
        bool bIsImporting = theWizard.field("isImporting").toBool();
        bool bIsCreating  = theWizard.field("isCreating").toBool();

        if (bIsImporting)
        {
            bool bIsURL      = theWizard.field("isURL").toBool();
            bool bIsFilename = theWizard.field("isFilename").toBool();
            bool bIsContents = theWizard.field("isContents").toBool();

            if (bIsURL)
            {
                QString qstrURL = theWizard.field("URL").toString();
                // --------------------------------
                if (qstrURL.isEmpty())
                {
                    QMessageBox::warning(this, tr("URL is Empty"),
                        tr("No URL was provided."));

                    return;
                }

                QUrl theURL(qstrURL);
                // --------------------------------
                if (NULL != m_pDownloader)
                {
                    delete m_pDownloader;
                    m_pDownloader = NULL;
                }
                // --------------------------------
                m_pDownloader = new FileDownloader(theURL, this);

                connect(m_pDownloader, SIGNAL(downloaded()), SLOT(DownloadedURL()));
            }
            // --------------------------------
            else if (bIsFilename)
            {
                QString fileName = theWizard.field("Filename").toString();

                if (fileName.isEmpty())
                {
                    QMessageBox::warning(this, tr("Filename is Empty"),
                        tr("No filename was provided."));

                    return;
                }
                // -----------------------------------------------
                QString qstrContents;
                QFile plainFile(fileName);

                if (plainFile.open(QIODevice::ReadOnly))//| QIODevice::Text)) // Text flag translates /n/r to /n
                {
                    QTextStream in(&plainFile); // Todo security: check filesize here and place a maximum size.
                    qstrContents = in.readAll();

                    plainFile.close();
                    // ----------------------------
                    if (qstrContents.isEmpty())
                    {
                        QMessageBox::warning(this, tr("File Was Empty"),
                                             QString("%1: %2").arg(tr("File was apparently empty")).arg(fileName));
                        return;
                    }
                    // ----------------------------
                }
                else
                {
                    QMessageBox::warning(this, tr("Failed Reading File"),
                                         QString("%1: %2").arg(tr("Failed trying to read file")).arg(fileName));
                    return;
                }
                // -----------------------------------------------
                ImportContract(qstrContents);
            }
            // --------------------------------
            else if (bIsContents)
            {
                QString qstrContents = theWizard.getContents();

                if (qstrContents.isEmpty())
                {
                    QMessageBox::warning(this, tr("Empty Contract"),
                        tr("Failure Importing: Contract is Empty."));
                    return;
                }
                // -------------------------
                ImportContract(qstrContents);
            }
        }
        // --------------------------------
        else if (bIsCreating)
        {
            // Todo
        }
    }
}

// ------------------------------------------------------

//virtual
void MTAssetDetails::refresh(QString strID, QString strName)
{
//  qDebug() << "MTAssetDetails::refresh";

    if (NULL != ui)
    {
        ui->lineEditID  ->setText(strID);
        ui->lineEditName->setText(strName);

        FavorLeftSideForIDs();
    }
}

// ------------------------------------------------------

void MTAssetDetails::on_lineEditName_editingFinished()
{
    if (!m_pOwner->m_qstrCurrentID.isEmpty())
    {
        bool bSuccess = OTAPI_Wrap::SetAssetType_Name(m_pOwner->m_qstrCurrentID.toStdString(),  // Asset Type
                                                      ui->lineEditName->text(). toStdString()); // New Name
        if (bSuccess)
        {
            m_pOwner->m_map.remove(m_pOwner->m_qstrCurrentID);
            m_pOwner->m_map.insert(m_pOwner->m_qstrCurrentID, ui->lineEditName->text());

            m_pOwner->SetPreSelected(m_pOwner->m_qstrCurrentID);

            m_pOwner->RefreshRecords();
        }
    }
}

// ------------------------------------------------------

