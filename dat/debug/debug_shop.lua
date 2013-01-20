-- Debug shop script
function BootShopTest()
    print("BootShopTest");

    GlobalManager:AddCharacter(BRONANN);
    GlobalManager:AddCharacter(KALYA);
    GlobalManager:AddCharacter(SYLVE);
    GlobalManager:AddCharacter(THANIS);

    -- Initial character money
    GlobalManager:AddDrunes(1842);

    -- Initial inventory
    GlobalManager:AddToInventory(1, 5);
    GlobalManager:AddToInventory(1001, 2);
    GlobalManager:AddToInventory(10001, 1);
    GlobalManager:AddToInventory(20002, 1);
    GlobalManager:AddToInventory(30001, 1);
    GlobalManager:AddToInventory(40001, 1);
    GlobalManager:AddToInventory(50001, 1);

    local shop = hoa_shop.ShopMode();

    shop:AddObject(1, 12);
    shop:AddObject(1001, 5);
    shop:AddObject(10001, 2);
    shop:AddObject(10002, 4);
    shop:AddObject(10003, 12);
    shop:AddObject(10004, 1);
    shop:AddObject(11001, 5);
    shop:AddObject(20002, 1);
    shop:AddObject(20003, 2);
    shop:AddObject(30001, 3);
    shop:AddObject(30002, 10);
    shop:AddObject(30003, 11);
    shop:AddObject(40001, 2);
    shop:AddObject(40002, 1);
    shop:AddObject(40003, 1);
    shop:AddObject(40004, 1);
    shop:AddObject(50001, 1);
    shop:AddObject(50002, 1);

    ModeManager:Push(shop, false, false);

end
