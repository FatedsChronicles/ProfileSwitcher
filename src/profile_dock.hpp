#pragma once

#include <QWidget>
#include <QListWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QGroupBox>
#include <QFormLayout>
#include <QSpinBox>
#include <QComboBox>
#include <QCheckBox>

class ProfileDockWidget : public QWidget {
	Q_OBJECT
public:
	explicit ProfileDockWidget(QWidget *parent = nullptr);
	void refreshProfiles();

private slots:
	void onSwitch();
	void onCreate();
	void onDuplicate();
	void onRename();
	void onDelete();
	void onApplyQuickSettings();

private:
	// UI
	QListWidget *listProfiles{};
	QPushButton *btnSwitch{};
	QPushButton *btnCreate{};
	QPushButton *btnDuplicate{};
	QPushButton *btnRename{};
	QPushButton *btnDelete{};
	QPushButton *btnApply{};

	// Create / Rename inputs
	QLineEdit *editNewName{};
	QLineEdit *editRenameTo{};

	// Quick settings (limited)
	// Stream
	QLineEdit *editRtmpServer{};
	QLineEdit *editStreamKey{};
	// Video
	QSpinBox  *spinBaseW{};
	QSpinBox  *spinBaseH{};
	QSpinBox  *spinOutW{};
	QSpinBox  *spinOutH{};
	QSpinBox  *spinFpsNum{};
	QSpinBox  *spinFpsDen{};
	// Output
	QComboBox *comboOutputMode{};
	QLineEdit *editSimpleEncoder{};

	// Helpers
	QString selectedProfile() const;
	bool confirmDanger(const QString &title, const QString &text);
	void loadCurrentSettingsIntoForm();
};
