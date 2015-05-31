#include "codeselectdialog.h"
#include "ui_codeselectdialog.h"
#include <QDebug>

CodeSelectDialog::CodeSelectDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::CodeSelectDialog)
{
    ui->setupUi(this);

    /* Connect event signals */
    connect(ui->bitsbtn8, SIGNAL(clicked()), this, SLOT(settings_changed()));
    connect(ui->bitsbtn16, SIGNAL(clicked()), this, SLOT(settings_changed()));
    connect(ui->bitsbtn32, SIGNAL(clicked()), this, SLOT(settings_changed()));
    connect(ui->varnameText, SIGNAL(textChanged(QString)), this, SLOT(settings_changed()));
    connect(ui->progmemCheck, SIGNAL(toggled(bool)), this, SLOT(settings_changed()));

    setMode(OPEN);
}

CodeSelectDialog::~CodeSelectDialog() {
    delete ui;
}

void CodeSelectDialog::setMode(Mode mode) {
    this->mode = mode;
    if (this->mode == OPEN) {
        /*
         * Open mode: grey out the bit selection/variable boxes
         * The code field is allowed to be edited
         */
        ui->bitBox->setEnabled(false);
        ui->varBox->setEnabled(false);
        ui->codeEdit->setReadOnly(false);
        this->setWindowTitle("Load data from code");
        detect_settings();
    } else if (this->mode == SAVE) {
        /*
         * Save mode: all settings are allowed to be changed
         * The code field is read-only as this is auto-generated
         */
        ui->bitBox->setEnabled(true);
        ui->varBox->setEnabled(true);
        ui->codeEdit->setReadOnly(true);
        this->setWindowTitle("Save data as code");
        apply_settings();
    }
}

/* Use the applied settings to convert the data into the right format */
void CodeSelectDialog::setData(QByteArray &data) {
    this->data = data;
    if (this->mode == SAVE) {
        apply_settings();
    }

    /* Update size text label */
    QString sizeText;
    sizeText.append("Data size: ");
    sizeText.append(QString::number(this->data.length()));
    sizeText.append(" bytes");
    this->ui->datasizeLbl->setText(sizeText);
}

/* Fired if any of the dialog settings are changed */
void CodeSelectDialog::settings_changed() {
    if (this->mode == SAVE) {
        apply_settings();
    }
}

/* Fired if the input text field is changed */
void CodeSelectDialog::on_codeEdit_textChanged() {
    if (this->mode == OPEN) {
        detect_settings();
    }
}

/* Detects the contents of the text-field, filling the data */
void CodeSelectDialog::detect_settings() {
    QString text = ui->codeEdit->toPlainText();
    text = text.trimmed();

    /*
     * First of all, trim the header of the text data looking for information
     * In case none is found, the largest found hexidecimal size is used instead
     */
    QString header;
    int data_start = 0;
    for (int i = 0; i < text.length(); i++) {
        QChar c = text.at(i);
        if (c == '{') {
            data_start = i + 1;
            break;
        } else if (header.length() == 0 && c.isDigit()) {
            data_start = i;
            break;
        } else {
            header.append(c);
        }
    }

    /* Start at the data index and collect all the data entries */
    QList<quint32> entries;
    entries.reserve(text.length() / 3);
    QString entry_buff;
    int data_size = 1; /* Amount of bytes to store data entry */
    for (int i = data_start; i < text.length(); i++) {
        QChar c = text.at(i);
        bool is_delim = (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == ',');
        bool is_end = (c == '}') || (i == (text.length() - 1));

        /* Add a new entry by parsing it as hex (0xFF) or decimal (255) */
        if ((is_delim || is_end) && entry_buff.length() > 0) {
            quint32 value;
            bool ok = false;
            if (entry_buff.startsWith("0x")) {
                entry_buff.remove(0, 2);
                value = (quint32) entry_buff.toLong(&ok, 16);
            } else {
                value = (quint32) entry_buff.toLong(&ok, 10);
            }
            if (ok) {
                int s = 1;
                if (value & (0xFF << 8)) s = 2;
                if (value & (0xFFFF << 16)) s = 4;
                if (s > data_size) data_size = s;
                entries.append(value);
            }
            entry_buff.clear();
        }

        /* Handle special characters here */
        if (is_delim) continue;
        if (is_end) break;

        /* Append to entry */
        entry_buff.append(c);
    }

    /* Parse the header to adjust the data size and other such settings */
    bool is_progmem = header.contains("PROGMEM");
    if (is_progmem) header.remove("PROGMEM");
    QString data_tbl[4][4] = {
        {"byte", "short", "", ""},
        {"char ", "int ", "", "long "},
        {"int8_t ", "int16_t ", "", "int32_t "},
        {"uint8_t ", "uint16_t ", "", "uint32_t "}
    };
    for (int i = 0; i < 4; i++) {
        for (int d = 0; d < 4; d++) {
            QString &t = data_tbl[i][d];
            if (t.length() == 0) continue;
            int idx = header.indexOf(t);
            if (idx == -1) continue;

            /* Verify there is a space in front of the keyword */
            QChar b = (idx == 0) ? ' ' : header.at(idx-1);
            if (b == ' ' || b == '\t' || b == '\n' || b == '\r') {
                data_size = d+1;
            }
        }
    }
    /* Parse name by taking the last free-standing word */
    QChar delims[] = {' ', '\t', '\n', '\r', ',', ':', '[', ']', '='};
    QString varname;
    for (int i = header.length() - 1; i >= 0; i--) {
        QChar c = header.at(i);
        bool is_delim = false;
        for (int di = 0; di < (sizeof(delims) / sizeof(QChar)); di++) {
            is_delim |= (c == delims[di]);
        }
        if (is_delim && varname.length() > 0) {
            break;
        } else if (!is_delim) {
            varname.insert(0, c);
        }
    }

    /* Apply to UI */
    ui->progmemCheck->setChecked(is_progmem);
    ui->varnameText->setText(varname);
    if (data_size == 1) {
        ui->bitsbtn8->setChecked(true);
    } else if (data_size == 2) {
        ui->bitsbtn16->setChecked(true);
    } else if (data_size == 4) {
        ui->bitsbtn32->setChecked(true);
    }

    /* Knowing the data contents and data size, we can construct the byte array */
    QByteArray entry_data;
    entry_data.reserve(entries.length() * data_size);
    for (int i = 0; i < entries.length(); i++) {
        quint32 value = entries.at(i);
        entry_data.append((char*) &value, data_size);
    }
    setData(entry_data);
}

/* Applies the data and settings producing the code */
void CodeSelectDialog::apply_settings() {
    QString text;

    /* First go through the main options and prepare the variable text */
    text.append("const unsigned ");
    int data_size = 1;
    int bytes_per_line = 8;
    if (ui->bitsbtn8->isChecked()) {
        data_size = 1;
        bytes_per_line = 8;
        text.append("char");
    } else if (ui->bitsbtn16->isChecked()) {
        data_size = 2;
        bytes_per_line = 4;
        text.append("int");
    } else if (ui->bitsbtn32->isChecked()) {
        data_size = 4;
        bytes_per_line = 4;
        text.append("long");
    }
    text.append(" ").append(ui->varnameText->text());
    if (ui->progmemCheck->isChecked()) {
        text.append(" PROGMEM");
    }
    text.append(" = {");

    /* Add all bytes, limiting it to a given bytes per line */
    const int space_offset = 2;
    int total_entries = ceil((double) data.length() / (double) data_size);
    unsigned char entry_buff[4];
    int data_index = 0;
    int entry_index = 0;
    while (total_entries--) {
        if (total_entries && (entry_index % bytes_per_line) == 0) {
            text.append("\n");
            text.append(QString(space_offset, ' '));
        }
        entry_index++;

        /* Grab the next entry of data */
        for (int i = 0; i < sizeof(entry_buff); i++) {
            if (i >= data_size || data_index >= data.length()) {
                entry_buff[i] = 0x00;
            } else {
                entry_buff[i] = data.at(data_index++);
            }
        }

        /* Convert raw data to uint32 */
        quint32 value = *((quint32*) entry_buff);

        /* Encode this data as a hexidecimal String */
        text.append("0x");
        text.append(QString("%1").arg(value, data_size * 2, 16, QLatin1Char('0')).toUpper());
        if (total_entries) {
            text.append(", ");
        }
    }

    /* Closing brace */
    text.append("\n};");

    /* Set text in code field */
    ui->codeEdit->setPlainText(text);
}
