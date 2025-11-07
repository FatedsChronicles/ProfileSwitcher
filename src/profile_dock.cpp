#include "profile_dock.hpp"
#include "log.hpp"

#include <obs-frontend-api.h>
#include <obs.h>
#include <util/config-file.h>

static inline QString qstr(const char *c) { return c ? QString::fromUtf8(c) : QString(); }

// -------- helpers

bool ProfileDockWidget::profileExists(const QString &name)
{
	char **profiles = obs_frontend_get_profiles();
	if (!profiles)
		return false;
	bool found = false;
	for (char **p = profiles; *p; ++p) {
		if (name == qstr(*p)) { found = true; break; }
	}
	bfree(profiles);
	return found;
}

QString ProfileDockWidget::selectedProfile() const
{
	auto *item = listProfiles->currentItem();
	return item ? item->text() : QString();
}

bool ProfileDockWidget::confirmDanger(const QString &title, const QString &text)
{
	return QMessageBox::question(this, title, text,
	                             QMessageBox::Yes|QMessageBox::No, QMessageBox::No)
	       == QMessageBox::Yes;
}

bool ProfileDockWidget::outputsActiveWarn()
{
	bool streaming = obs_frontend_streaming_active();
	bool recording = obs_frontend_recording_active();
	if (streaming || recording) {
		QMessageBox::warning(this, tr("Active Outputs"),
		                     tr("Streaming or recording is active. Stop all outputs before applying Video changes (resolution/FPS)."));
		return true;
	}
	return false;
}

// -------- ctor + UI

ProfileDockWidget::ProfileDockWidget(QWidget *parent) : QWidget(parent)
{
	auto *root = new QVBoxLayout(this);

	// Profile list
	listProfiles = new QListWidget(this);
	root->addWidget(listProfiles, /*stretch*/1);

	// Profile actions
	{
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

		auto *row2 = new QHBoxLayout();
		editNewName   = new QLineEdit(this); editNewName->setPlaceholderText(tr("New profile name"));
		editRenameTo  = new QLineEdit(this); editRenameTo->setPlaceholderText(tr("Rename to..."));
		row2->addWidget(new QLabel(tr("Create:"))); row2->addWidget(editNewName);
		row2->addSpacing(12);
		row2->addWidget(new QLabel(tr("Rename:"))); row2->addWidget(editRenameTo);
		root->addLayout(row2);
	}

	// Quick Settings (Stream, Video, Output)
	{
		auto *grp = new QGroupBox(tr("Quick Settings (Stream, Video, Output)"), this);
		auto *form = new QFormLayout();

		// Stream
		editRtmpServer = new QLineEdit(grp);
		editRtmpServer->setPlaceholderText("rtmp://live.twitch.tv/app");
		editStreamKey  = new QLineEdit(grp);
		editStreamKey->setPlaceholderText("sk_xxx...");
		editStreamKey->setEchoMode(QLineEdit::Password);
		form->addRow(new QLabel(tr("RTMP Server:")), editRtmpServer);
		form->addRow(new QLabel(tr("Stream Key:")),  editStreamKey);

		// Video (Base/Output + FPS)
		spinBaseW = new QSpinBox(grp); spinBaseW->setRange(16, 16384); spinBaseW->setValue(1920);
		spinBaseH = new QSpinBox(grp); spinBaseH->setRange(16, 16384); spinBaseH->setValue(1080);
		spinOutW  = new QSpinBox(grp); spinOutW->setRange(16, 16384);  spinOutW->setValue(1920);
		spinOutH  = new QSpinBox(grp); spinOutH->setRange(16, 16384);  spinOutH->setValue(1080);
		spinFpsNum= new QSpinBox(grp); spinFpsNum->setRange(1, 1000);  spinFpsNum->setValue(60);
		spinFpsDen= new QSpinBox(grp); spinFpsDen->setRange(1, 1000);  spinFpsDen->setValue(1);

		// Resolution row
		{
			auto *row = new QHBoxLayout();
			row->addWidget(new QLabel("Base:"));  row->addWidget(spinBaseW);
			row->addWidget(new QLabel("x"));      row->addWidget(spinBaseH);
			row->addSpacing(8);
			row->addWidget(new QLabel("Output:"));row->addWidget(spinOutW);
			row->addWidget(new QLabel("x"));      row->addWidget(spinOutH);
			auto *field = new QWidget(grp); field->setLayout(row);
			form->addRow(new QLabel(tr("Resolution:")), field);
		}
		// FPS row
		{
			auto *row = new QHBoxLayout();
			row->addWidget(new QLabel("Num:")); row->addWidget(spinFpsNum);
			row->addSpacing(8);
			row->addWidget(new QLabel("Den:")); row->addWidget(spinFpsDen);
			auto *field = new QWidget(grp); field->setLayout(row);
			form->addRow(new QLabel(tr("FPS (num/den):")), field);
		}

		// Output
		comboOutputMode = new QComboBox(grp);
		comboOutputMode->addItems({ "Simple", "Advanced" });
		editSimpleEncoder = new QLineEdit(grp);
		editSimpleEncoder->setPlaceholderText("x264 / h264 / hevc / qsv_h264 ...");
		form->addRow(new QLabel(tr("Output Mode:")),    comboOutputMode);
		form->addRow(new QLabel(tr("Simple Encoder:")), editSimpleEncoder);

		btnLoad  = new QPushButton(tr("Load From Current"), grp);
		btnApply = new QPushButton(tr("Apply Quick Settings to Selected"), grp);
		auto *actions = new QHBoxLayout();
		actions->addWidget(btnLoad);
		actions->addStretch();
		actions->addWidget(btnApply);

		auto *wrap = new QVBoxLayout();
		wrap->addLayout(form);
		wrap->addLayout(actions);
		grp->setLayout(wrap);
		root->addWidget(grp);
	}

	// wire signals
	connect(btnSwitch,    &QPushButton::clicked, this, &ProfileDockWidget::onSwitch);
	connect(btnCreate,    &QPushButton::clicked, this, &ProfileDockWidget::onCreate);
	connect(btnDuplicate, &QPushButton::clicked, this, &ProfileDockWidget::onDuplicate);
	connect(btnRename,    &QPushButton::clicked, this, &ProfileDockWidget::onRename);
	connect(btnDelete,    &QPushButton::clicked, this, &ProfileDockWidget::onDelete);

	connect(btnLoad,      &QPushButton::clicked, this, &ProfileDockWidget::onLoadFromCurrent);
	connect(btnApply,     &QPushButton::clicked, this, &ProfileDockWidget::onApplyQuickSettings);

	refreshProfiles();
	loadCurrentSettingsIntoForm();
}

// -------- profiles

void ProfileDockWidget::refreshProfiles()
{
	listProfiles->clear();
	char **profiles = obs_frontend_get_profiles();
	if (!profiles) return;
	for (char **p = profiles; *p; ++p) listProfiles->addItem(qstr(*p));
	bfree(profiles);

	// select current
	if (char *cur = obs_frontend_get_current_profile()) {
		auto items = listProfiles->findItems(qstr(cur), Qt::MatchExactly);
		if (!items.isEmpty()) listProfiles->setCurrentItem(items.first());
		bfree(cur);
	}
}

void ProfileDockWidget::onSwitch()
{
	const auto name = selectedProfile();
	if (name.isEmpty()) return;
	obs_frontend_set_current_profile(name.toUtf8().constData());
}

void ProfileDockWidget::onCreate()
{
	const auto name = editNewName->text().trimmed();
	if (name.isEmpty()) return;

	obs_frontend_create_profile(name.toUtf8().constData());
	if (!profileExists(name)) {
		QMessageBox::warning(this, tr("Create Profile"),
		                     tr("Failed to create profile (it may already exist or name is invalid)."));
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
	obs_frontend_duplicate_profile(copyName.toUtf8().constData());
	if (!profileExists(copyName)) {
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
	                     .arg(src, dst)))
		return;

	obs_frontend_set_current_profile(src.toUtf8().constData());
	obs_frontend_duplicate_profile(dst.toUtf8().constData());
	if (!profileExists(dst)) {
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

// -------- settings

void ProfileDockWidget::onLoadFromCurrent()
{
	loadCurrentSettingsIntoForm();
	QMessageBox::information(this, tr("Loaded"), tr("Loaded settings from the current profile."));
}

void ProfileDockWidget::loadCurrentSettingsIntoForm()
{
	config_t *cfg = obs_frontend_get_profile_config();
	if (!cfg) return;

	// Video
	int baseW = config_get_int(cfg, "Video", "BaseCX");
	int baseH = config_get_int(cfg, "Video", "BaseCY");
	int outW  = config_get_int(cfg, "Video", "OutputCX");
	int outH  = config_get_int(cfg, "Video", "OutputCY");
	int fpsN  = config_get_int(cfg, "Video", "FPSTypeNumerator");
	int fpsD  = config_get_int(cfg, "Video", "FPSTypeDenominator");
	if (baseW>0) spinBaseW->setValue(baseW);
	if (baseH>0) spinBaseH->setValue(baseH);
	if (outW>0)  spinOutW->setValue(outW);
	if (outH>0)  spinOutH->setValue(outH);
	if (fpsN>0)  spinFpsNum->setValue(fpsN);
	if (fpsD>0)  spinFpsDen->setValue(fpsD);

	// Output
	const char *mode = config_get_string(cfg, "Output", "Mode");
	const char *simpleEnc = config_get_string(cfg, "SimpleOutput", "StreamEncoder");
	if (mode) {
		int idx = comboOutputMode->findText(qstr(mode), Qt::MatchFixedString);
		if (idx >= 0) comboOutputMode->setCurrentIndex(idx);
	}
	if (simpleEnc) editSimpleEncoder->setText(qstr(simpleEnc));

	// Stream: not re-reading key for privacy
}

bool ProfileDockWidget::applySettingsToProfile(const QString &profileName)
{
	obs_frontend_set_current_profile(profileName.toUtf8().constData());

	config_t *cfg = obs_frontend_get_profile_config();
	if (!cfg) return false;

	// --- Video
	config_set_int(cfg, "Video", "BaseCX", spinBaseW->value());
	config_set_int(cfg, "Video", "BaseCY", spinBaseH->value());
	config_set_int(cfg, "Video", "OutputCX", spinOutW->value());
	config_set_int(cfg, "Video", "OutputCY", spinOutH->value());
	config_set_int(cfg, "Video", "FPSTypeNumerator",   spinFpsNum->value());
	config_set_int(cfg, "Video", "FPSTypeDenominator", spinFpsDen->value());

	// --- Output
	config_set_string(cfg, "Output", "Mode",
	                  comboOutputMode->currentText().toUtf8().constData());
	if (!editSimpleEncoder->text().trimmed().isEmpty()) {
		config_set_string(cfg, "SimpleOutput", "StreamEncoder",
		                  editSimpleEncoder->text().trimmed().toUtf8().constData());
	}

	config_save(cfg);
	obs_frontend_save();

	// --- Stream (optional)
	const auto server = editRtmpServer->text().trimmed();
	const auto key    = editStreamKey->text().trimmed();
	if (!server.isEmpty() || !key.isEmpty()) {
		if (server.isEmpty() || key.isEmpty()) {
			QMessageBox::warning(this, tr("Stream Settings"),
			                     tr("If setting Stream, both RTMP Server and Stream Key are required."));
			return false;
		}
		obs_data_t *settings = obs_data_create();
		obs_data_set_string(settings, "server", server.toUtf8().constData());
		obs_data_set_string(settings, "key",    key.toUtf8().constData());
		obs_service_t *svc = obs_service_create("rtmp_common", "ProfileSwitcherService", settings, nullptr);
		if (svc) {
			obs_frontend_set_streaming_service(svc);
			obs_frontend_save_streaming_service();
			obs_service_release(svc);
		} else {
			QMessageBox::warning(this, tr("Stream Settings"),
			                     tr("Failed to create RTMP service. Check server URL/key."));
			obs_data_release(settings);
			return false;
		}
		obs_data_release(settings);
	}

	return true;
}

void ProfileDockWidget::onApplyQuickSettings()
{
	const auto name = selectedProfile();
	if (name.isEmpty()) {
		QMessageBox::information(this, tr("Apply Settings"), tr("Select a profile first."));
		return;
	}
	if (outputsActiveWarn())
		return;

	if (!applySettingsToProfile(name)) return;

	obs_frontend_reset_video(); // requires outputs inactive
	QMessageBox::information(this, tr("Apply Settings"),
	                         tr("Settings applied to '%1'.").arg(name));
}
