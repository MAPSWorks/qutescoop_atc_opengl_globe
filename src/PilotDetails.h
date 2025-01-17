/**************************************************************************
 *  This file is part of QuteScoop. See README for license
 **************************************************************************/

#ifndef PILOTDETAILS_H_
#define PILOTDETAILS_H_

#include "ui_PilotDetails.h" // file generated by UIC from PilotDetails.ui

#include "ClientDetails.h"
#include "Pilot.h"

class PilotDetails : public ClientDetails, private Ui::PilotDetails {
        Q_OBJECT
    public:
        static PilotDetails* instance(bool createIfNoInstance = true, QWidget *parent = 0);
        void destroyInstance();
        virtual void refresh(Pilot *pilot = 0);
    protected:
        void closeEvent(QCloseEvent *event);
    private slots:
        void on_cbPlotRoute_clicked(bool checked);
        void on_buttonAlt_clicked();
        void on_buttonDest_clicked();
        void on_buttonFrom_clicked();
        void on_buttonAddFriend_clicked();

        // @todo move to ClientDetails
        void on_pbAlias_clicked();

    private:
        PilotDetails(QWidget *parent);
        Pilot *_pilot;
};

#endif /*PILOTDETAILS_H_*/
