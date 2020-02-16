#include "../globals.h"
#include "../game_main.h"
#include "../npc.h"
#include "../load_gfx.h"
#include "../custom.h"
#include "../sound.h"

#include <DirManager/dirman.h>
#include <Utils/files.h>
#include <PGE_File_Formats/file_formats.h>

void OpenWorld(std::string FilePath)
{
    // USE PGE-FL here
    std::string newInput = "";
    int FileRelease = 0;
    int A = 0;
    int B = 0;
    WorldData wld;

    ClearWorld();

    FileFormats::OpenWorldFile(FilePath, wld);

//    for(A = FilePath.length(); A >= 1; A--)
//    {
//        if(FilePath.substr(A - 1, 1) == "/" || FilePath.substr(A - 1, 1) == "\\")
//            break;
//    }

    FileName = wld.meta.filename; //FilePath.substr(FilePath.length() - (FilePath.length() - A));
    FileNamePath = wld.meta.path + "/"; //FilePath.substr(0, (A));

    LoadWorldCustomGFX();

    numTiles = 0;
    numScenes = 0;
    numWorldLevels = 0;
    numWorldPaths = 0;
    numWorldMusic = 0;

    WorldName = wld.EpisodeTitle;
    wld.charactersToS64();
    blockCharacter[1] = wld.nocharacter1;
    blockCharacter[2] = wld.nocharacter2;
    blockCharacter[3] = wld.nocharacter3;
    blockCharacter[4] = wld.nocharacter4;
    blockCharacter[5] = wld.nocharacter5;
    StartLevel = wld.IntroLevel_file;
    NoMap = wld.HubStyledWorld;
    RestartLevel = wld.restartlevel;

    MaxWorldStars = int(wld.stars);

    for(int i = 1; i <= maxWorldCredits; i++)
        WorldCredits[i].clear();

    B = 0;
    for(auto &c : wld.authors)
    {
        B++;
        if(B > maxWorldCredits)
            break;
        WorldCredits[B] = c;
    }

    for(auto &t : wld.tiles)
    {
        numTiles++;
        if(numTiles > maxTiles)
        {
            numTiles = maxTiles;
            break;
        }

        Tile[numTiles].Location.X = t.x;
        Tile[numTiles].Location.Y = t.y;
        Tile[numTiles].Type = int(t.id);
        Tile[numTiles].Location.Width = TileWidth[Tile[numTiles].Type];
        Tile[numTiles].Location.Height = TileHeight[Tile[numTiles].Type];
    }

    for(auto &s : wld.scenery)
    {
        numScenes++;
        if(numScenes > maxScenes)
        {
            numScenes = maxScenes;
            break;
        }
        Scene[numScenes].Type = int(s.id);
        Scene[numScenes].Location.X = s.x;
        Scene[numScenes].Location.Y = s.y;
        Scene[numScenes].Location.Width = SceneWidth[Scene[numScenes].Type];
        Scene[numScenes].Location.Height = SceneHeight[Scene[numScenes].Type];
        Scene[numScenes].Active = true;
    }

    for(auto &p : wld.paths)
    {
        numWorldPaths++;
        if(numWorldPaths > maxWorldPaths)
        {
            numWorldPaths = maxWorldPaths;
            break;
        }
        WorldPath[numWorldPaths].Location.X = p.x;
        WorldPath[numWorldPaths].Location.Y = p.y;
        WorldPath[numWorldPaths].Type = int(p.id);
        WorldPath[numWorldPaths].Location.Width = 32;
        WorldPath[numWorldPaths].Location.Height = 32;
//        if(LevelEditor == true)
//            WorldPath[numWorldPaths].Active = true;
    }

    for(auto &l : wld.levels)
    {
        numWorldLevels++;
        if(numWorldLevels > maxWorldLevels)
        {
            numWorldLevels = maxWorldLevels;
            break;
        }

        WorldLevel[numWorldLevels].Location.X = l.x;
        WorldLevel[numWorldLevels].Location.Y = l.y;
        WorldLevel[numWorldLevels].Type = int(l.id);
        WorldLevel[numWorldLevels].FileName = l.lvlfile;
        WorldLevel[numWorldLevels].LevelName = l.title;
        WorldLevel[numWorldLevels].LevelExit[1] = l.top_exit;
        WorldLevel[numWorldLevels].LevelExit[2] = l.left_exit;
        WorldLevel[numWorldLevels].LevelExit[3] = l.bottom_exit;
        WorldLevel[numWorldLevels].LevelExit[4] = l.right_exit;
        WorldLevel[numWorldLevels].Location.Width = 32;
        WorldLevel[numWorldLevels].Location.Height = 32;
        WorldLevel[numWorldLevels].StartWarp = int(l.entertowarp);
        WorldLevel[numWorldLevels].Visible = l.alwaysVisible;
        WorldLevel[numWorldLevels].Active = WorldLevel[numWorldLevels].Visible;
        WorldLevel[numWorldLevels].Path = l.pathbg;
        WorldLevel[numWorldLevels].Start = l.gamestart;
        WorldLevel[numWorldLevels].WarpX = l.gotox;
        WorldLevel[numWorldLevels].WarpY = l.gotoy;
        WorldLevel[numWorldLevels].Path2 = l.bigpathbg;
    }

    for(auto &m : wld.music)
    {
        numWorldMusic++;
        if(numWorldMusic > maxWorldMusic)
        {
            numWorldMusic = maxWorldMusic;
            break;
        }

        WorldMusic[numWorldMusic].Location.X = m.x;
        WorldMusic[numWorldMusic].Location.Y = m.y;
        WorldMusic[numWorldMusic].Type = int(m.id);

        // In game they are smaller (30x30), in world they are 32x32
        WorldMusic[numWorldMusic].Location.Width = 30;
        WorldMusic[numWorldMusic].Location.Height = 30;
        WorldMusic[numWorldMusic].Location.Y = WorldMusic[numWorldMusic].Location.Y + 1;
        WorldMusic[numWorldMusic].Location.X = WorldMusic[numWorldMusic].Location.X + 1;
    }

    LoadCustomGFX();

//    if(LevelEditor == false)
    {
        for(A = 1; A <= numWorldLevels; A++)
        {
            if((FileRelease <= 20 && WorldLevel[A].Type == 1) || (FileRelease > 20 && WorldLevel[A].Start == true))
            {
                WorldPlayer[1].Type = 1;
                WorldPlayer[1].Location = WorldLevel[A].Location;
                break;
            }
        }
        for(A = 1; A <= numWorldLevels; A++)
        {
            if((FileRelease <= 20 && WorldLevel[A].Type == 1) || (FileRelease > 20 && WorldLevel[A].Start == true))
            {
                WorldLevel[A].Active = true;
                LevelPath(A, 5, true);
            }
        }
    }
//    else
//    {
//        for(A = 1; A <= numCharacters; A++)
//        {
//            if(blockCharacter[A] == true)
//                frmWorld::chkChar(A).Value = 1;
//            else
//                frmWorld::chkChar(A).Value = 0;
//        }
//        frmWorld.txtWorldName = WorldName;
//        frmWorld.txtStartLevel = StartLevel;
//        if(NoMap == true)
//            frmWorld::chkNoMap.Value = 1;
//        else
//            frmWorld::chkNoMap.Value = 0;
//        if(RestartLevel == true)
//            frmWorld::chkRestartLevel.Value = 1;
//        else
//            frmWorld::chkRestartLevel.Value = 0;
//        for(A = 1; A <= 5; A++)
//            frmWorld::txtCredits(A).Text = WorldCredits[A];
//        frmWorld.txtStars = MaxWorldStars;
//    }
    overTime = 0;
    GoalTime = SDL_GetTicks() + 1000;
    fpsCount = 0;
    fpsTime = 0;
    cycleCount = 0;
    gameTime = 0;
}

void ClearWorld()
{
    Tile_t blankTile;
    WorldPath_t blankPath;
    Scene_t blankScene;
    WorldLevel_t blankLevel;
    WorldMusic_t blankMusic;
    int A = 0;

    for(A = 1; A <= numCharacters; A++)
    {
        blockCharacter[A] = false;
//        if(LevelEditor == true)
//            frmWorld::chkChar(A).Value = 0;
    }

    for(A = 1; A <= numTiles; A++)
        Tile[A] = blankTile;
    for(A = 1; A <= numWorldPaths; A++)
        WorldPath[A] = blankPath;
    for(A = 1; A <= numScenes; A++)
        Scene[A] = blankScene;
    for(A = 1; A <= numWorldLevels; A++)
        WorldLevel[A] = blankLevel;
    for(A = 1; A <= numWorldMusic; A++)
        WorldMusic[A] = blankMusic;

    MaxWorldStars = 0;
    numTiles = 0;
    numWorldPaths = 0;
    numScenes = 0;
    numWorldLevels = 0;
    numWorldPaths = 0;
    numWorldMusic = 0;
    RestartLevel = false;
    NoMap = false;
    StartLevel = "";
    BeatTheGame = false;
    for(A = 1; A <= 5; A++)
        WorldCredits[A] = "";
    UnloadCustomGFX();
    UnloadWorldCustomGFX();
//    if(LevelEditor == true)
//    {
//        frmLevelEditor::optCursor(14).Value = true;
//        frmWorld.txtWorldName = "";
//        frmWorld.txtStartLevel = "";
//        frmWorld::chkNoMap.Value = false;
//        frmWorld.chkRestartLevel = false;
//        vScreenX[1] = 0;
//        vScreenY[1] = 0;
//        for(A = 1; A <= 5; A++)
//            frmWorld::txtCredits(A).Text = "";
//        frmWorld.txtStars = "";
//        MaxWorldStars = 0;
//    }
}