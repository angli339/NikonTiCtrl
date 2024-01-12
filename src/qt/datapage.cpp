#include "qt/datapage.h"

#include <QVBoxLayout>
#include <fmt/format.h>
#include <fmt/ostream.h>
#include <sstream>
#include <xtensor/xbuilder.hpp>
#include <xtensor/xmath.hpp>

DataPage::DataPage(QWidget *parent) : QWidget(parent)
{
    QVBoxLayout *layout = new QVBoxLayout(this);

    refresh_button = new QPushButton("Refresh");
    refresh_button->setFixedSize(50, 30);

    table = new QTableWidget;
    table->setColumnCount(6);
    table->setHorizontalHeaderLabels(
        {"Plate", "Well", "# Sites", "# Images", "# Images Quant.", "# Cells"});

    layout->addWidget(refresh_button);
    layout->addWidget(table);

    QObject::connect(refresh_button, &QPushButton::pressed, this,
                     &DataPage::refreshContent);
}

void DataPage::setExperimentControlModel(ExperimentControlModel *model)
{
    this->model = model;
    QObject::connect(model, &ExperimentControlModel::quantificationUpdated,
                     this, &DataPage::refreshContent);
}

void DataPage::refreshContent()
{
    table->clear();

    // Find out total # of rows & columns
    int n_rows = 0;
    QStringList channels;
    auto exp = this->model->Experiment();
    for (const auto &plate : exp->Samples()->Plates()) {
        n_rows += plate->NumEnabledWells();
        for (const auto &well : plate->EnabledWells()) {
            std::vector<NDImage *> ims =
                exp->Images()->ListNDImage(plate->ID(), well->ID());
            for (const auto &im : ims) {
                for (const auto &ch_name : im->ChannelNames()) {
                    if (!channels.contains(ch_name.c_str())) {
                        channels.push_back(ch_name.c_str());
                    }
                }
            }
        }
    }

    QStringList header_labels = {
        "Plate",           "Well",    "# Sites",      "# Images",
        "# Images Quant.", "# Cells", "# Cells/Image"};
    header_labels.append(channels);

    table->setRowCount(n_rows);
    table->setColumnCount(header_labels.size());
    table->setHorizontalHeaderLabels(header_labels);

    // Fill in the rows
    int i_row = 0;
    for (const auto &plate : exp->Samples()->Plates()) {
        auto enabled_wells = plate->EnabledWells();

        for (int i_well = 0; i_well < enabled_wells.size(); i_well++, i_row++) {
            auto well = enabled_wells[i_well];
            std::vector<NDImage *> ims =
                exp->Images()->ListNDImage(plate->ID(), well->ID());

            std::vector<std::string> ch_names;
            std::vector<xt::xarray<float>> raw_intensity_mean;
            int n_quantified = 0;
            int n_cells = 0;
            for (const auto &im : ims) {
                if (exp->Analysis()->HasQuantification(im->Name(), 0)) {
                    n_quantified++;
                    QuantificationResults quantification =
                        exp->Analysis()->GetQuantification(im->Name(), 0);
                    n_cells += quantification.region_props.size();
                    if (raw_intensity_mean.empty()) {
                        ch_names = quantification.ch_names;
                        raw_intensity_mean = quantification.raw_intensity_mean;
                    } else {
                        for (int i_ch = 0; i_ch < ch_names.size(); i_ch++) {
                            raw_intensity_mean[i_ch] = xt::hstack(xt::xtuple(
                                raw_intensity_mean[i_ch],
                                quantification.raw_intensity_mean[i_ch]));
                        }
                    }
                }
            }

            auto set_col_fn = [this, i_well](int i_col, std::string value) {
                QTableWidgetItem *item = new QTableWidgetItem(value.c_str());
                table->setItem(i_well, i_col, item);
            };

            table->setRowHeight(i_well, 15);
            set_col_fn(0, plate->ID());
            set_col_fn(1, well->ID());
            set_col_fn(2, fmt::format("{}", well->NumEnabledSites()));
            if (ims.size() > 0) {
                set_col_fn(3, fmt::format("{}", ims.size()));
            }
            if (n_quantified > 0) {
                set_col_fn(4, fmt::format("{}", n_quantified));
            }
            if (n_cells > 0) {
                set_col_fn(5, fmt::format("{}", n_cells));
            }
            if (n_quantified > 0) {
                set_col_fn(6, fmt::format("{:.1f}", (float)n_cells /
                                                        (float)n_quantified));
            }

            if ((!raw_intensity_mean.empty()) &&
                (ch_names.size() == raw_intensity_mean.size()))
            {
                for (int i_ch = 0; i_ch < ch_names.size(); i_ch++) {
                    double ch_mean = xt::pow(
                        10, xt::mean(xt::log10(raw_intensity_mean[i_ch])))[0];
                    int i_col = header_labels.indexOf(ch_names[i_ch].c_str());
                    set_col_fn(i_col, fmt::format("{:.1f}", ch_mean));
                }
            }
        }
    }
}
