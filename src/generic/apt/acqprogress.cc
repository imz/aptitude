// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: acqprogress.cc,v 1.1.2.2 2002/02/23 15:33:25 daniel Exp $
/* ######################################################################

   Acquire Progress - Command line progress meter 
   
   ##################################################################### */
									/*}}}*/
// Include files							/*{{{*/
#include "acqprogress.h"

#include "download_signal_log.h"

#include <aptitude.h>
#include <apt-pkg/acquire-item.h>
#include <apt-pkg/acquire-worker.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/error.h>

#include <stdio.h>
#include <signal.h>
#include <iostream>
									/*}}}*/

using namespace std;

// FIXME -- DNB -- make the Update=1 lines do something sensible.

// AcqTextStatus::AcqTextStatus - Constructor				/*{{{*/
// ---------------------------------------------------------------------
/* */
AcqTextStatus::AcqTextStatus(unsigned int &ScreenWidth,unsigned int Quiet) :
    ScreenWidth(ScreenWidth), Quiet(Quiet)
{
}


AcqTextStatus::~AcqTextStatus()
{
}

									/*}}}*/
// AcqTextStatus::Start - Downloading has started			/*{{{*/
// ---------------------------------------------------------------------
/* */
void AcqTextStatus::Start(download_signal_log &manager) 
{
   BlankLine[0] = 0;
   ID = 1;
};
									/*}}}*/
// AcqTextStatus::IMSHit - Called when an item got a HIT response	/*{{{*/
// ---------------------------------------------------------------------
/* */
void AcqTextStatus::IMSHit(pkgAcquire::ItemDesc &Itm,
			   download_signal_log &manager)
{
   if (Quiet > 1)
      return;

   if (Quiet <= 0)
      cout << '\r' << BlankLine << '\r';   
   
   cout << _("Hit ") << Itm.Description;
   if (Itm.Owner->FileSize != 0)
      cout << " [" << SizeToStr(Itm.Owner->FileSize) << "B]";
   cout << endl;
   manager.set_update(true);
};
									/*}}}*/
// AcqTextStatus::Fetch - An item has started to download		/*{{{*/
// ---------------------------------------------------------------------
/* This prints out the short description and the expected size */
void AcqTextStatus::Fetch(pkgAcquire::ItemDesc &Itm, download_signal_log &manager)
{
  manager.set_update(true);
   if (Itm.Owner->Complete == true)
      return;
   
   Itm.Owner->ID = ID++;
   
   if (Quiet > 1)
      return;

   if (Quiet <= 0)
      cout << '\r' << BlankLine << '\r';
   
   cout << _("Get:") << Itm.Owner->ID << ' ' << Itm.Description;
   if (Itm.Owner->FileSize != 0)
      cout << " [" << SizeToStr(Itm.Owner->FileSize) << "B]";
   cout << endl;
};
									/*}}}*/
// AcqTextStatus::Done - Completed a download				/*{{{*/
// ---------------------------------------------------------------------
/* We don't display anything... */
void AcqTextStatus::Done(pkgAcquire::ItemDesc &Itm, download_signal_log &manager)
{
  manager.set_update(true);
};
									/*}}}*/
// AcqTextStatus::Fail - Called when an item fails to download		/*{{{*/
// ---------------------------------------------------------------------
/* We print out the error text  */
void AcqTextStatus::Fail(pkgAcquire::ItemDesc &Itm, download_signal_log &manager)
{
   if (Quiet > 1)
      return;

   // Ignore certain kinds of transient failures (bad code)
   if (Itm.Owner->Status == pkgAcquire::Item::StatIdle)
      return;
      
   if (Quiet <= 0)
      cout << '\r' << BlankLine << '\r';
   
   if (Itm.Owner->Status == pkgAcquire::Item::StatDone)
   {
      cout << _("Ign ") << Itm.Description << endl;
   }
   else
   {
      cout << _("Err ") << Itm.Description << endl;
      cout << "  " << Itm.Owner->ErrorText << endl;
   }
   
   manager.set_update(true);
};
									/*}}}*/
// AcqTextStatus::Stop - Finished downloading				/*{{{*/
// ---------------------------------------------------------------------
/* This prints out the bytes downloaded and the overall average line
   speed */
void AcqTextStatus::Stop(download_signal_log &manager,
			 const sigc::slot0<void> &k)
{
   if (Quiet > 1)
     {
       k();
       return;
     }

   if (Quiet <= 0)
      cout << '\r' << BlankLine << '\r' << flush;

   if (manager.get_fetched_bytes() != 0 && _error->PendingError() == false)
      ioprintf(cout,_("Fetched %sB in %s (%sB/s)\n"),
	       SizeToStr(manager.get_fetched_bytes()).c_str(),
	       TimeToStr(manager.get_elapsed_time()).c_str(),
	       SizeToStr(manager.get_currentCPS()).c_str());

   k();
}
									/*}}}*/
// AcqTextStatus::Pulse - Regular event pulse				/*{{{*/
// ---------------------------------------------------------------------
/* This draws the current progress. Each line has an overall percent
   meter and a per active item status meter along with an overall 
   bandwidth and ETA indicator. */
void AcqTextStatus::Pulse(pkgAcquire *Owner, download_signal_log &manager,
			  const sigc::slot1<void, bool> &k)
{
  TotalBytes=manager.get_total_bytes();
  TotalItems=manager.get_total_items();
  CurrentBytes=manager.get_current_bytes();
  CurrentItems=manager.get_current_items();
  CurrentCPS=manager.get_currentCPS();

   if (Quiet > 0)
     {
       k(true);
       return;
     }
   
   enum {Long = 0,Medium,Short} Mode = Long;
   
   char Buffer[1024];
   char *End = Buffer + sizeof(Buffer);
   char *S = Buffer;
   if (ScreenWidth >= sizeof(Buffer))
      ScreenWidth = sizeof(Buffer)-1;

   // Put in the percent done
   snprintf(S,1024,"%ld%%",long(double((CurrentBytes + CurrentItems)*100.0)/double(TotalBytes+TotalItems)));

   bool Shown = false;
   for (pkgAcquire::Worker *I = Owner->WorkersBegin(); I != 0;
	I = Owner->WorkerStep(I))
   {
      S += strlen(S);
      
      // There is no item running 
      if (I->CurrentItem == 0)
      {
	 if (I->Status.empty() == false)
	 {
	    snprintf(S,End-S," [%s]",I->Status.c_str());
	    Shown = true;
	 }
	 
	 continue;
      }

      Shown = true;
      
      // Add in the short description
      if (I->CurrentItem->Owner->ID != 0)
	 snprintf(S,End-S," [%lu %s",I->CurrentItem->Owner->ID,
		  I->CurrentItem->ShortDesc.c_str());
      else
	 snprintf(S,End-S," [%s",I->CurrentItem->ShortDesc.c_str());
      S += strlen(S);

      // Show the short mode string
      if (I->CurrentItem->Owner->Mode != 0)
      {
	 snprintf(S,End-S," %s",I->CurrentItem->Owner->Mode);
	 S += strlen(S);
      }
            
      // Add the current progress
      if (Mode == Long)
	 snprintf(S,End-S," %lu",I->CurrentSize);
      else
      {
	 if (Mode == Medium || I->TotalSize == 0)
	    snprintf(S,End-S," %sB",SizeToStr(I->CurrentSize).c_str());
      }
      S += strlen(S);
      
      // Add the total size and percent
      if (I->TotalSize > 0 && I->CurrentItem->Owner->Complete == false)
      {
	 if (Mode == Short)
	    snprintf(S,End-S," %lu%%",
		     long(double(I->CurrentSize*100.0)/double(I->TotalSize)));
	 else
	    snprintf(S,End-S,"/%sB %lu%%",SizeToStr(I->TotalSize).c_str(),
		     long(double(I->CurrentSize*100.0)/double(I->TotalSize)));
      }      
      S += strlen(S);
      snprintf(S,End-S,"]");
   }

   // Show something..
   if (Shown == false)
      snprintf(S,End-S,_(" [Working]"));
      
   /* Put in the ETA and cps meter, block off signals to prevent strangeness
      during resizing */
   sigset_t Sigs,OldSigs;
   sigemptyset(&Sigs);
   sigaddset(&Sigs,SIGWINCH);
   sigprocmask(SIG_BLOCK,&Sigs,&OldSigs);
   
   if (CurrentCPS != 0)
   {      
      char Tmp[300];
      unsigned long ETA = (unsigned long)((TotalBytes - CurrentBytes)/CurrentCPS);
      sprintf(Tmp," %sB/s %s",SizeToStr(CurrentCPS).c_str(),TimeToStr(ETA).c_str());
      unsigned int Len = strlen(Buffer);
      unsigned int LenT = strlen(Tmp);
      if (Len + LenT < ScreenWidth)
      {	 
	 memset(Buffer + Len,' ',ScreenWidth - Len);
	 strcpy(Buffer + ScreenWidth - LenT,Tmp);
      }      
   }
   Buffer[ScreenWidth] = 0;
   BlankLine[ScreenWidth] = 0;
   sigprocmask(SIG_SETMASK,&OldSigs,0);

   // Draw the current status
   if (strlen(Buffer) == strlen(BlankLine))
      cout << '\r' << Buffer << flush;
   else
      cout << '\r' << BlankLine << '\r' << Buffer << flush;
   memset(BlankLine,' ',strlen(Buffer));
   BlankLine[strlen(Buffer)] = 0;
   
   manager.set_update(false);

   k(true);
}
									/*}}}*/
// AcqTextStatus::MediaChange - Media need to be swapped		/*{{{*/
// ---------------------------------------------------------------------
/* Prompt for a media swap */
void AcqTextStatus::MediaChange(string Media, string Drive,
				download_signal_log &manager,
				const sigc::slot1<void, bool> &k)
{
   if (Quiet <= 0)
      cout << '\r' << BlankLine << '\r';
   ioprintf(cout,_("Media Change: Please insert the disc labeled '%s' in "
		   "the drive '%s' and press [Enter].\n"),
	    Media.c_str(),Drive.c_str());

   char C = 0;
   while (C != '\n' && C != '\r')
     if(read(STDIN_FILENO,&C,1) == -1)
       {
	 k(false);
	 return;
       }
   
   manager.set_update(true);
   k(true);
}
									/*}}}*/

