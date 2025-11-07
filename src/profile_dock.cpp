#include "profile_dock.hpp"
#include "log.hpp"

#include <obs-frontend-api.h>
#include <obs.h>
#include <util/config-file.h>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QLabel>

static inline QString qstr(const char *c) { return c ? QString::fromUtf8(c) : QString(); }

ProfileDockWidget::ProfileDockWidget(QWidget *parent) : QWidget(parent)
{
	auto *root = new QVBoxLayout(this);

	// --- Profile list + actions
	listProfiles = new QListWidget(this);
	root->addWidget(listProfiles, /*stretch*/1);

	auto *row1 = new QHBoxLayout();
	btnSwitch    = new QPushButton(tr("Switch"), this);
	btnCreate    = new QPushButton(tr("Create"), this);
	btnDuplicate = new QPushButton(tr("Duplicate"), this);
	btnRename    = new QPushButton(tr("Rename"), this);
	btnDelete    = new QPushButton(tr("Delete"), this);
	row1->addWidget(btnSwitch);
	row1->addWidget(btnCreate);
	row1->addWidget(btnDuplicate);
	row1->addWidget(btnRename);
	row1->addWidget(btnDelete);
	root->addLayout(row1);

	// Create / Rename fields
	auto *row2 = new QHBoxLayout();
	editNewName   = new QLineEdit(this); editNewName->setPlaceholderText(tr("New profile name"));
	editRenameTo  = new QLineEdit(this); editRenameTo->setPlaceholderText(tr("Rename to..."));
	row2->addWidget(new QLabel(tr("Create:"))); row2->addWidget(editNewName);
	row2->addSpacing(12);
	row2->addWidget(new QLabel(tr("Rename:"))); row2->addWidget(editRenameTo);
	root->addLayout(row2);

	// --- Quick settings group
	auto *grp = new QGroupBox(tr("Quick Settings (Stream, Video, Output)"), this);
	auto *form = new QFormLayout(grp);

	// Stream (optional)
	editRtmpServer = new QLineEdit(grp); editRtmpServer->setPlaceholderText("rtmp://live.twitch.tv/app");
	editStreamKey  = new QLineEdit(grp); editStreamKey->setPlaceholderText("sk_xxx..."); editStreamKey->setEchoMode(QLineEdit::Password);
	form->addRow(new QLabel(tr("RTMP Server:")), editRtmpServer);
	form->addRow(new QLabel(tr("Stream Key:")), editStreamKey);

	// Video
	spinBaseW = new QSpinBox(grp);  spinBaseW->setRange(16, 16384); spinBaseW->setValue(1920);
	spinBaseH = new QSpinBox(grp);  spinBaseH->setRange(16, 16384); spinBaseH->setValue(1080);
	spinOutW  = new QSpinBox(grp);  spinOutW->setRange(16, 16384);  spinOutW->setValue(1920);
	spinOutH  = new QSpinBox(grp);  spinOutH->setRange(16, 16384);  spinOutH->setValue(1080);
	spinFpsNum= new QSpinBox(grp);  spinFpsNum->setRange(1, 1000);  spinFpsNum->setValue(60);
	spinFpsDen= new QSpinBox(grp);  spinFpsDen->setRange(1, 1000);  spinFpsDen->setValue(1);
	auto *videoRow1 = new QHBoxLayout();
	videoRow1->addWidget(new QLabel("Base:"));  videoRow1->addWidget(spinBaseW); videoRow1->addWidget(new QLabel("x")); videoRow1->addWidget(spinBaseH);
	videoRow1->addSpacing(8);
	videoRow1->addWidget(new QLabel("Output:")); videoRow1->addWidget(spinOutW); videoRow1->addWidget(new QLabel("x")); videoRow1->addWidget(spinOutH);
	form->addRow(new QLabel(tr("Resolution:")), new QWidget(grp)); // spacer
	form->setLayout( form->rowCount()-1, QFormLayout::FieldRole, videoRow1 );
	auto *fpsRow = new QHBoxLayout();
	fpsRow->addWidget(new QLabel("Num:")); fpsRow->addWidget(spinFpsNum);
	fpsRow->addSpacing(8);
	fpsRow->addWidget(new QLabel("Den:")); fpsRow->addWidget(spinFpsDen);
	form->addRow(new QLabel(tr("FPS (num/den):")), new QWidget(grp));
	form->setLayout(form->rowCount()-1, QFormLayout::FieldRole, fpsRow);

	// Output
	comboOutputMode = new QComboBox(grp);
	comboOutputMode->addItems({ "Simple", "Advanced" });
	editSimpleEncoder = new QLineEdit(grp); editSimpleEncoder->setPlaceholderText("x264 / h264 / hevc / qsv_h264 ...");
	form->addRow(new QLabel(tr("Output Mode:")), comboOutputMode);
	form->addRow(new QLabel(tr("Simple Encoder:")), editSimpleEncoder);

	btnApply = new QPushButton(tr("Apply Quick Settings to Selected"), grp);
	auto *applyRow = new QHBoxLayout();
	applyRow->addStretch();
	applyRow->addWidget(btnApply);
	auto *wrap = new QVBoxLayout();
	wrap->addLayout(form);
	wrap->addLayout(applyRow);
	grp->setLayout(wrap);
	root->addWidget(grp);

	// Signals
	connect(btnSwitch,    &QPushButton::clicked, this, &ProfileDockWidget::onSwitch);
	connect(btnCreate,    &QPushButton::clicked, this, &ProfileDockWidget::onCreate);
	connect(btnDuplicate, &QPushButton::clicked, this, &ProfileDockWidget::onDuplicate);
	connect(btnRename,    &QPushButton::clicked, this, &ProfileDockWidget::onRename);
	connect(btnDelete,    &QPushButton::clicked, this, &ProfileDockWidget::onDelete);
	connect(btnApply,     &QPushButton::clicked, this, &ProfileDockWidget::onApplyQuickSettings);

	refreshProfiles();
	loadCurrentSettingsIntoForm();
}

QString ProfileDockWidget::selectedProfile() const
{
	auto *item = listProfiles->currentItem();
	return item ? item->text() : QString();
}

bool ProfileDockWidget::confirmDanger(const QString &title, const QString &text)
{
	return QMessageBox::question(this, title, text, QMessageBox::Yes|QMessageBox::No, QMessageBox::No) == QMessageBox::Yes;
}

void ProfileDockWidget::refreshProfiles()
{
	listProfiles->clear();
	char **profiles = obs_frontend_get_profiles();
	if (!profiles) return;
	for (char **p = profiles; *p; ++p) {
		listProfiles->addItem(qstr(*p));
	}
	bfree(profiles);
	// Select current
	char *cur = obs_frontend_get_current_profile();
	if (cur) {
		auto items = listProfiles->findItems(qstr(cur), Qt::MatchExactly);
		if (!items.isEmpty()) listProfiles->setCurrentItem(items.first());
		bfree(cur);
	}
}

void ProfileDockWidget::onSwitch()
{
	auto name = selectedProfile();
	if (name.isEmpty()) return;
	obs_frontend_set_current_profile(name.toUtf8().constData());
}

void ProfileDockWidget::onCreate()
{
	const auto name = editNewName->text().trimmed();
	if (name.isEmpty()) return;

	if (!obs_frontend_create_profile(name.toUtf8().constData())) {
		QMessageBox::warning(this, tr("Create Profile"), tr("Failed to create profile (name may already exist)."));
		return;
	}
	obs_frontend_set_current_profile(name.toUtf8().constData());
	refreshProfiles();
}

void ProfileDockWidget::onDuplicate()
{
	const auto base = selectedProfile();
	const auto copyName = editNewName->text().trimmed();
	if (base.isEmpty() || copyName.isEmpty()) return;

	obs_frontend_set_current_profile(base.toUtf8().constData());
	if (!obs_frontend_duplicate_profile(copyName.toUtf8().constData())) {
		QMessageBox::warning(this, tr("Duplicate Profile"), tr("Failed to duplicate profile."));
		return;
	}
	refreshProfiles();
}

void ProfileDockWidget::onRename()
{
	const auto src = selectedProfile();
	const auto dst = editRenameTo->text().trimmed();
	if (src.isEmpty() || dst.isEmpty()) return;

	if (!confirmDanger(tr("Rename Profile"),
	                   tr("This will duplicate '%1' to '%2' and then delete '%1'. Continue?")
	                   .arg(src, dst))) return;

	obs_frontend_set_current_profile(src.toUtf8().constData());
	if (!obs_frontend_duplicate_profile(dst.toUtf8().constData())) {
		QMessageBox::warning(this, tr("Rename Profile"), tr("Duplicate step failed."));
		return;
	}

	if (src != dst) {
		obs_frontend_delete_profile(src.toUtf8().constData());
		obs_frontend_set_current_profile(dst.toUtf8().constData());
	}
	refreshProfiles();
}

void ProfileDockWidget::onDelete()
{
	const auto name = selectedProfile();
	if (name.isEmpty()) return;

	if (!confirmDanger(tr("Delete Profile"),
	                   tr("Delete profile '%1'? This cannot be undone.").arg(name)))
		return;

	obs_frontend_delete_profile(name.toUtf8().constData());
	refreshProfiles();
}

void ProfileDockWidget::loadCurrentSettingsIntoForm()
{
	config_t *cfg = obs_frontend_get_profile_config();
	if (!cfg) return;

	int baseW = config_get_int(cfg, "Video", "BaseCX");
	int baseH = config_get_int(cfg, "Video", "BaseCY");
	int outW  = config_get_int(cfg, "Video", "OutputCX");
	int outH  = config_get_int(cfg, "Video", "OutputCY");
	int fpsN  = config_get_int(cfg, "Video", "FPSTypeNumerator");
	int fpsD  = config_get_int(cfg, "Video", "FPSTypeDenominator");
	const char *mode = config_get_string(cfg, "Output", "Mode");
	const char *simpleEnc = config_get_string(cfg, "SimpleOutput", "StreamEncoder");

	if (baseW>0) spinBaseW->setValue(baseW);
	if (baseH>0) spinBaseH->setValue(baseH);
	if (outW>0)  spinOutW->setValue(outW);
	if (outH>0)  spinOutH->setValue(outH);
	if (fpsN>0)  spinFpsNum->setValue(fpsN);
	if (fpsD>0)  spinFpsDen->setValue(fpsD);
	if (mode) {
		int idx = comboOutputMode->findText(qstr(mode), Qt::MatchFixedString);
		if (idx >= 0) comboOutputMode->setCurrentIndex(idx);
	}
	if (simpleEnc) editSimpleEncoder->setText(qstr(simpleEnc));
}

void ProfileDockWidget::onApplyQuickSettings()
{
	const auto name = selectedProfile();
	if (name.isEmpty()) {
		QMessageBox::information(this, tr("Apply Settings"), tr("Select a profile first."));
		return;
	}

	obs_frontend_set_current_profile(name.toUtf8().constData());

	config_t *cfg = obs_frontend_get_profile_config();
	if (!cfg) {
		QMessageBox::warning(this, tr("Apply Settings"), tr("Could not get profile config."));
		return;
	}

	// --- Video (Base/Output/FPS)
	config_set_int(cfg, "Video", "BaseCX", spinBaseW->value());
	config_set_int(cfg, "Video", "BaseCY", spinBaseH->value());
	config_set_int(cfg, "Video", "OutputCX", spinOutW->value());
	config_set_int(cfg, "Video", "OutputCY", spinOutH->value());
	config_set_int(cfg, "Video", "FPSTypeNumerator",   spinFpsNum->value());
	config_set_int(cfg, "Video", "FPSTypeDenominator", spinFpsDen->value());

	// --- Output
	config_set_string(cfg, "Output", "Mode", comboOutputMode->currentText().toUtf8().constData());
	if (!editSimpleEncoder->text().trimmed().isEmpty()) {
		config_set_string(cfg, "SimpleOutput", "StreamEncoder",
		                  editSimpleEncoder->text().trimmed().toUtf8().constData());
	}

	config_save(cfg);
	obs_frontend_save();
	obs_frontend_reset_video(); // requires no active outputs

	// --- Stream service (optional)
	const auto server = editRtmpServer->text().trimmed();
	const auto key    = editStreamKey->text().trimmed();
	if (!server.isEmpty() && !key.isEmpty()) {
		obs_data_t *settings = obs_data_create();
		obs_data_set_string(settings, "server", server.toUtf8().constData());
		obs_data_set_string(settings, "key",    key.toUtf8().constData());
		obs_service_t *svc = obs_service_create("rtmp_common", "ProfileSwitcherService", settings, nullptr);
		if (svc) {
			obs_frontend_set_streaming_service(svc);
			obs_frontend_save_streaming_service();
			PLOG(LOG_INFO, "Updated streaming service for profile '%s'", name.toUtf8().constData());
			obs_service_release(svc);
		} else {
			QMessageBox::warning(this, tr("Stream Service"),
			                     tr("Failed to create RTMP service. Check server URL/key."));
		}
		obs_data_release(settings);
	}

	QMessageBox::information(this, tr("Apply Settings"), tr("Settings applied to '%1'.").arg(name));
}
