#pragma once

class AIShip;

class AIShipCameraController
{
  private:
    ATTRIBUTES *pTargetAPointer;
    uint32_t dwTarget;
    float fDelta;
    int32_t iCrosshairTex;
    // read from Crosshair.OutsideCamera of the sea cameras; without the attribute the camera aims
    // along the view
    bool bCameraOutside = false;

    uint32_t Colors[4]{};

    uint32_t dwSubTexturesX = 1, dwSubTexturesY = 1;

    AIShip *pOurAIShip;
    ATTRIBUTES *pACrosshair;

    bool isCameraOutside() const
    {
        return bCameraOutside;
    };

  public:
    void Realize(float fDeltaTime);
    void Execute(float fDeltaTime);

    bool Fire();
    bool Init();

    AIShipCameraController(AIShip *);
    ~AIShipCameraController();

    void SetAIShip(AIShip *pShip)
    {
        pOurAIShip = pShip;
    }

    AIShip *GetAIShip() const
    {
        return pOurAIShip;
    }

    void Save(CSaveLoad *pSL) const;
    void Load(CSaveLoad *pSL);
};
