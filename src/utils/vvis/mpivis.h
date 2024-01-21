//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef MPIVIS_H
#define MPIVIS_H
#ifdef _WIN32
#pragma once
#endif


void VVIS_SetupMPI( int &argc, char **&argv );


void RunMPIBasePortalVis();
void RunMPIPortalFlow();

// DistributeWork owns this packet ID.
#define VMPI_DISTRIBUTEWORK_PACKETID			2

#endif // MPIVIS_H
