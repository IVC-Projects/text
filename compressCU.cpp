Void TEncCu::xCompressCU( TComDataCU*& rpcBestCU, TComDataCU*& rpcTempCU, const UInt uiDepth DEBUG_STRING_FN_DECLARE(sDebug_), PartSize eParentPartSize )
{
    TComPic* pcPic = rpcBestCU->getPic(); // 获取当前CU的图像
    const TComPPS &pps=*(rpcTempCU->getSlice()->getPPS()); // 获取图像参数集
    const TComSPS &sps=*(rpcTempCU->getSlice()->getSPS()); // 获取序列参数集
    
    m_ppcOrigYuv[uiDepth]->copyFromPicYuv( pcPic->getPicYuvOrg(), rpcBestCU->getCtuRsAddr(), rpcBestCU->getZorderIdxInCtu() ); // 从图像中获取原始YUV数据
    
    Bool    doNotBlockPu = true; // 快速cbf标识（cbf模式见术语表）
    Bool    earlyDetectionSkipMode = false; //early skip早期跳出标识（early skip模式见术语表）
    
    const UInt uiLPelX   = rpcBestCU->getCUPelX(); // 最左端点x坐标
    const UInt uiRPelX   = uiLPelX + rpcBestCU->getWidth(0)  - 1; // 最右端点x坐标
    const UInt uiTPelY   = rpcBestCU->getCUPelY(); // 最上端点y坐标
    const UInt uiBPelY   = uiTPelY + rpcBestCU->getHeight(0) - 1; // 最下端点y坐标
    const UInt uiWidth   = rpcBestCU->getWidth(0); // 当前CU块宽度
    
    Int iBaseQP = xComputeQP( rpcBestCU, uiDepth );
    // 传入当前CU和深度，计算对当前CU的QP；如果不是对每个CU自适应的改变QP，则直接用之前slice算出的QP
    
    const UInt numberValidComponents = rpcBestCU->getPic()->getNumberValidComponents();
    // 获取成分数量，如果色度格式是CHROMA_400，数量为1，反之为3（最大）
    
    /* 【省略代码】根据当前深度、是否使用码率控制、是否使用TQB（TransquantBypass模式，见术语表）调整QP最大和最小的范围（iMinQP-iMaxQP） */
    
    TComSlice * pcSlice = rpcTempCU->getPic()->getSlice(rpcTempCU->getPic()->getCurrSliceIdx()); // 获取当前所在slice
    
    const Bool bBoundary = !( uiRPelX < sps.getPicWidthInLumaSamples() && uiBPelY < sps.getPicHeightInLumaSamples() ); // 当前CU块的右边界在整个图像的最右边 或者 下边界在整个图像最下边 则为TRUE（即在边界）
    
    if ( !bBoundary ) // 如果不在边界
    {
        for (Int iQP=iMinQP; iQP<=iMaxQP; iQP++) // 在之前确定的QP范围中枚举QP
        {
            /* 【省略代码】如果是TransquantBypass模式（这里用bIsLosslessMode布尔型标识）且如果当前枚举到最小QP，将其改为lowestQP */
            /* 【省略代码】如果是自适应改变QP，设置相关的对最小编码块大小取Log的值、色度QP偏移量索引*/
            
            rpcTempCU->initEstData( uiDepth, iQP, bIsLosslessMode );
            // 使用CTU四叉树子层的deltaQP初始化预测数据，根据深度设置CU的宽度和高度，对QP赋值
            
            /* {做帧间预测, SKIP和2Nx2N} */
            if( rpcBestCU->getSlice()->getSliceType() != I_SLICE )
            {
                /* {2Nx2N} */
                if(m_pcEncCfg->getUseEarlySkipDetection()) // 使用early skip早期跳出模式
                {
                    xCheckRDCostInter( rpcBestCU, rpcTempCU, SIZE_2Nx2N DEBUG_STRING_PASS_INTO(sDebug) ); // 尝试用普通模式进行预测
                    rpcTempCU->initEstData( uiDepth, iQP, bIsLosslessMode ); // rpcBestCU保存性能最优的预测方式下的参数，rpcTempCU是每次用于尝试划分预测的CU，每次做完后重新恢复初始化
                }
                /* {SKIP} */
                xCheckRDCostMerge2Nx2N( rpcBestCU, rpcTempCU DEBUG_STRING_PASS_INTO(sDebug), &earlyDetectionSkipMode ); // 尝试用Merge模式进行预测，传入早期跳出标识，如果模式为skip则修改该布尔值
                rpcTempCU->initEstData( uiDepth, iQP, bIsLosslessMode );
                
                if(!m_pcEncCfg->getUseEarlySkipDetection())
                {
                    /* {2Nx2N, NxN（?讲道理，真没找到这个NxN哪里做了）} */
                    xCheckRDCostInter( rpcBestCU, rpcTempCU, SIZE_2Nx2N DEBUG_STRING_PASS_INTO(sDebug) );
                    rpcTempCU->initEstData( uiDepth, iQP, bIsLosslessMode );
                    if(m_pcEncCfg->getUseCbfFastMode()) // 使用快速cbf模式
                    {
                        doNotBlockPu = rpcBestCU->getQtRootCbf( 0 ) != 0; // 判断四叉树根节点的CBFlag如果为true，则不需要后续继续划分
                    }
                }
            }
        }
        
        
        if(!earlyDetectionSkipMode) // 如果之前没有设置提前跳出，继续尝试所有的划分方式
        {
            for (Int iQP=iMinQP; iQP<=iMaxQP; iQP++) // 枚举QP
            {
                /* 【省略代码】如果是TransquantBypass模式同上处理 */
                rpcTempCU->initEstData( uiDepth, iQP, bIsLosslessMode ); // CU恢复初始化
                
                /* {帧间预测, NxN, 2NxN, Nx2N} */
                if( rpcBestCU->getSlice()->getSliceType() != I_SLICE )
                {
                    /* {2Nx2N, NxN} */
                    if(!( (rpcBestCU->getWidth(0)==8) && (rpcBestCU->getHeight(0)==8) )) // 当前CU划分到最小（8*8）
                    {
                        if( uiDepth == sps.getLog2DiffMaxMinCodingBlockSize() && doNotBlockPu) // 如果当前块的深度为当前的四叉树底层 且不满足跳出快速cbf条件
                        {
                            xCheckRDCostInter( rpcBestCU, rpcTempCU, SIZE_NxN DEBUG_STRING_PASS_INTO(sDebug)   ); // 做NxN的普通预测
                            rpcTempCU->initEstData( uiDepth, iQP, bIsLosslessMode );
                        }
                    }
                    /* {Nx2N} */
                    if(doNotBlockPu) // 不满足跳出快速cbf条件
                    {
                        xCheckRDCostInter( rpcBestCU, rpcTempCU, SIZE_Nx2N DEBUG_STRING_PASS_INTO(sDebug)  );
                        rpcTempCU->initEstData( uiDepth, iQP, bIsLosslessMode );
                        if(m_pcEncCfg->getUseCbfFastMode() && rpcBestCU->getPartitionSize(0) == SIZE_Nx2N ) // 如果使用快速CBF策略 且（刚刚尝试的）Nx2N是最佳的划分
                        {
                            doNotBlockPu = rpcBestCU->getQtRootCbf( 0 ) != 0; // 判断四叉树根节点的CBFlag如果为true，则不需要后续继续划分
                        }
                    }
                    /* {2NxN} */
                    if(doNotBlockPu) // 和上面一样，就不写了
                    {
                        xCheckRDCostInter      ( rpcBestCU, rpcTempCU, SIZE_2NxN DEBUG_STRING_PASS_INTO(sDebug)  );
                        rpcTempCU->initEstData( uiDepth, iQP, bIsLosslessMode );
                        if(m_pcEncCfg->getUseCbfFastMode() && rpcBestCU->getPartitionSize(0) == SIZE_2NxN)
                        {
                            doNotBlockPu = rpcBestCU->getQtRootCbf( 0 ) != 0;
                        }
                    }
                    
                    /* {尝试非对称分割 (SIZE_2NxnU, SIZE_2NxnD, SIZE_nLx2N, SIZE_nRx2N)} */
                    if(sps.getUseAMP() && uiDepth < sps.getLog2DiffMaxMinCodingBlockSize() ) // 如果允许非对称分割 且 当前块的深度不是当前的四叉树底层
                    {
                        /* 【省略代码】如果AMP_ENC_SPEEDUP（AMP编码加速）则根据之前尝试划分的最好情况省去尝试一些AMP的划分情况，以此达到加快编码的目的
                        否则，则朴素的尝试所有的AMP划分方式（这种情况的代码省去了，下面只解释加速情况下的代码）*/
                        Bool bTestAMP_Hor = false, bTestAMP_Ver = false; // 是否使用AMP横向划分，纵向划分的标识
                        /* 【省略代码】根据之前划分的最佳模式是横切或竖切或四分等判断使用横向或者竖向的非对称划分 */
                        /* 【省略代码】如果AMP_MRG（AMP Merge） 则增加一对bTestMergeAMP_Hor，bTestMergeAMP_Ver标识横向和纵向划分；
                         在AMG_MRG下，调用xCheckRDCostInter()时在最后增加一个布尔类型为真的参数，会在predInterSearch()函数中对传入的残差清零；
                         其他代码结构与非AMG_MRG相同，因此以下将AMG_MRG预编译判断内的部分都省去了 */
                        
                        /* {做横向的非对称运动分割} */
                        if ( bTestAMP_Hor ) // 如果可以进行横向AMP划分
                        {
                            /* {2NxnU} */
                            if(doNotBlockPu) // 和之前的对称划分的普通模式一样，只是传入参数PartSize改为相应的非对称划分
                            {
                                xCheckRDCostInter( rpcBestCU, rpcTempCU, SIZE_2NxnU DEBUG_STRING_PASS_INTO(sDebug) );                                rpcTempCU->initEstData( uiDepth, iQP, bIsLosslessMode );
                                if(m_pcEncCfg->getUseCbfFastMode() && rpcBestCU->getPartitionSize(0) == SIZE_2NxnU )
                                {
                                    doNotBlockPu = rpcBestCU->getQtRootCbf( 0 ) != 0;
                                }
                            }
                            /* {2NxnD} */
                            if(doNotBlockPu)
                            {
                                xCheckRDCostInter( rpcBestCU, rpcTempCU, SIZE_2NxnD DEBUG_STRING_PASS_INTO(sDebug) );
                                rpcTempCU->initEstData( uiDepth, iQP, bIsLosslessMode );
                                if(m_pcEncCfg->getUseCbfFastMode() && rpcBestCU->getPartitionSize(0) == SIZE_2NxnD )
                                {
                                    doNotBlockPu = rpcBestCU->getQtRootCbf( 0 ) != 0;
                                }
                            }
                        }
                        
                        /* {做纵向的非对称运动分割} */
                        if ( bTestAMP_Ver ) // 如果可以进行横向AMP划分
                        {
                            /* {nLx2N} */
                            if(doNotBlockPu)
                            {
                                xCheckRDCostInter( rpcBestCU, rpcTempCU, SIZE_nLx2N DEBUG_STRING_PASS_INTO(sDebug) );
                                rpcTempCU->initEstData( uiDepth, iQP, bIsLosslessMode );
                                if(m_pcEncCfg->getUseCbfFastMode() && rpcBestCU->getPartitionSize(0) == SIZE_nLx2N )
                                {
                                    doNotBlockPu = rpcBestCU->getQtRootCbf( 0 ) != 0;
                                }
                            }
                            /* {nRx2N} */
                            if(doNotBlockPu)
                            {
                                xCheckRDCostInter( rpcBestCU, rpcTempCU, SIZE_nRx2N DEBUG_STRING_PASS_INTO(sDebug) );
                                rpcTempCU->initEstData( uiDepth, iQP, bIsLosslessMode );
                            }
                        }
                        
                    } /* {结束：AMP} */
                    
                } /* {结束：帧间预测} */
                
                /* {帧内预测} */
                if( (rpcBestCU->getSlice()->getSliceType() == I_SLICE) ||
                    ( (!m_pcEncCfg->getDisableIntraPUsInInterSlices()) &&
                      (
                        ( rpcBestCU->getCbf( 0, COMPONENT_Y  ) != 0 ) ||
                        ( (rpcBestCU->getCbf( 0, COMPONENT_Cb ) != 0) && (numberValidComponents > COMPONENT_Cb) ) ||
                        ( (rpcBestCU->getCbf( 0, COMPONENT_Cr ) != 0) && (numberValidComponents > COMPONENT_Cr) )
                      )
                    )
                  ) // 如果是I帧 或者 允许做帧间预测且CU已被标记CBF（预测残差为0）
                {
                    xCheckRDCostIntra( rpcBestCU, rpcTempCU, SIZE_2Nx2N DEBUG_STRING_PASS_INTO(sDebug) ); // 尝试2Nx2N帧内预测
                    rpcTempCU->initEstData( uiDepth, iQP, bIsLosslessMode );
                    if( uiDepth == sps.getLog2DiffMaxMinCodingBlockSize() ) // 如果当前深度为四叉树最底层
                    {
                        if( rpcTempCU->getWidth(0) > ( 1 << sps.getQuadtreeTULog2MinSize() ) ) // 如果当前CU宽度大于最小的TU宽度
                        {
                            xCheckRDCostIntra( rpcBestCU, rpcTempCU, SIZE_NxN DEBUG_STRING_PASS_INTO(sDebug)   ); // 尝试NxN帧内预测
                            rpcTempCU->initEstData( uiDepth, iQP, bIsLosslessMode );
                        }
                    }
                } /* {结束：帧内预测} */
                
                /* {尝试PCM模式（PCM见术语表）} */
                if(sps.getUsePCM()
                   && rpcTempCU->getWidth(0) <= (1<<sps.getPCMLog2MaxSize())
                   && rpcTempCU->getWidth(0) >= (1<<sps.getPCMLog2MinSize()) ) // 如果允许PCM且当前CU的宽度在PCM最小到最大范围内
                {
                    UInt uiRawBits = getTotalBits(rpcBestCU->getWidth(0), rpcBestCU->getHeight(0), rpcBestCU->getPic()->getChromaFormat(), sps.getBitDepths().recon); // 直接传递整个CU像素的码率
                    UInt uiBestBits = rpcBestCU->getTotalBits(); // 对CU进行最佳预测编码的码率
                    if((uiBestBits > uiRawBits) || (rpcBestCU->getTotalCost() > m_pcRdCost->calcRdCost(uiRawBits, 0)))
                    { // 如果进行预测编码的码率大于传递整个CU像素的码率 或者 前者的RDO大于后者的RDO
                        xCheckIntraPCM (rpcBestCU, rpcTempCU); // 尝试使用PCM模式
                        rpcTempCU->initEstData( uiDepth, iQP, bIsLosslessMode );
                    }
                }
                
            } /* {结束：枚举iQP} */
        }/* {结束：尝试所有划分方式} */
        
        if( rpcBestCU->getTotalCost() != MAX_DOUBLE ) // 正在测试的配置没有超过最大字节数，进行熵编码
        {
            m_pcRDGoOnSbacCoder->load(m_pppcRDSbacCoder[uiDepth][CI_NEXT_BEST]);
            m_pcEntropyCoder->resetBits(); // 重置码率
            m_pcEntropyCoder->encodeSplitFlag( rpcBestCU, 0, uiDepth, true ); // 对分割标志进行编码
            rpcBestCU->getTotalBits() += m_pcEntropyCoder->getNumberOfWrittenBits(); // split bits
            rpcBestCU->getTotalBins() += ((TEncBinCABAC *)((TEncSbac*)m_pcEntropyCoder->m_pcEntropyCoderIf)->getEncBinIf())->getBinsCoded(); // 计算熵编码码率
            rpcBestCU->getTotalCost()  = m_pcRdCost->calcRdCost( rpcBestCU->getTotalBits(), rpcBestCU->getTotalDistortion() );
            m_pcRDGoOnSbacCoder->store(m_pppcRDSbacCoder[uiDepth][CI_NEXT_BEST]); // 计算总的RD Cost
        }
        
    } /* {结束：如果不在边界的判断} */
    
    if( rpcBestCU->getTotalCost()!=MAX_DOUBLE && rpcBestCU->isLosslessCoded(0) && (rpcBestCU->getIPCMFlag(0) == false))
    {
        xFillPCMBuffer(rpcBestCU, m_ppcOrigYuv[uiDepth]); // 将原始YUV样本复制到PCM缓冲区
    }
    
    /* 【省略代码】根据最大CUDeltaQP深度、是否使用码率控制调整QP最大和最小的范围（iMinQP-iMaxQP） */
    
    const Bool bSubBranch = bBoundary || !( m_pcEncCfg->getUseEarlyCU() && rpcBestCU->getTotalCost()!=MAX_DOUBLE && rpcBestCU->isSkipped(0) ); // 是否继续划分四叉树标识（Early CU见术语表）
    
    if( bSubBranch && uiDepth < sps.getLog2DiffMaxMinCodingBlockSize() && (!getFastDeltaQp() || uiWidth > fastDeltaQPCuMaxSize || bBoundary)) // 如果可以继续划分并且当前深度不在四叉树最底层
    {
        for (Int iQP=iMinQP; iQP<=iMaxQP; iQP++) // 枚举QP
        {
            UChar       uhNextDepth         = uiDepth+1; // 下一层的深度
            TComDataCU* pcSubBestPartCU     = m_ppcBestCU[uhNextDepth]; // 下一层的最好CU数组
            TComDataCU* pcSubTempPartCU     = m_ppcTempCU[uhNextDepth]; // 下一层的临时CU数组
            
            for ( UInt uiPartUnitIdx = 0; uiPartUnitIdx < 4; uiPartUnitIdx++ ) // 枚举划分四叉树的四个子块的下标
            {
                pcSubBestPartCU->initSubCU( rpcTempCU, uiPartUnitIdx, uhNextDepth, iQP ); // 清空或初始化BestCU子块的数据
                pcSubTempPartCU->initSubCU( rpcTempCU, uiPartUnitIdx, uhNextDepth, iQP ); // 清空或初始化TempCU子块的数据
                
                if( ( pcSubBestPartCU->getCUPelX() < sps.getPicWidthInLumaSamples() ) && ( pcSubBestPartCU->getCUPelY() < sps.getPicHeightInLumaSamples() ) ) // 子块CU的横纵坐标位置在亮度样本图像之内（可以继续往下迭代）
                {
                    if ( 0 == uiPartUnitIdx) // 如果迭代到第一块子块（左上角）
                    {
                        m_pppcRDSbacCoder[uhNextDepth][CI_CURR_BEST]->load(m_pppcRDSbacCoder[uiDepth][CI_CURR_BEST]); // 使用之前（当前深度）的缓存初始化RDO
                    }
                    else // 迭代其他子块
                    {
                        m_pppcRDSbacCoder[uhNextDepth][CI_CURR_BEST]->load(m_pppcRDSbacCoder[uhNextDepth][CI_NEXT_BEST]); // 使用现在（下一深度）的缓存初始RDO
                    }
                    
                    /* 【省略代码】如果使用AMP_ENC_SPEEDUP（与在AMP加速是同一个），在递归调用xCompressXU()时在最后增加一个PartSize参数
                     仅用于在加速的AMP决策时判断较优的划分方式用；总之不管使不使用AMP加速，这里都要递归调用子块的compressCU*/
                    
                    xCompressCU( pcSubBestPartCU, pcSubTempPartCU, uhNextDepth ); // 递归下一层的子块
                    
                    rpcTempCU->copyPartFrom( pcSubBestPartCU, uiPartUnitIdx, uhNextDepth ); // 将最好的子块的数据存在当前的临时数据中
                    xCopyYuv2Tmp( pcSubBestPartCU->getTotalNumPart()*uiPartUnitIdx, uhNextDepth ); // 复制预测图像和重建图像的YUV数据
                } /* {结束：可以继续往下迭代} */
                else
                {
                    pcSubBestPartCU->copyToPic( uhNextDepth ); // 将当前预测的部分复制到图片中的CU，用于预测下一个子块
                    rpcTempCU->copyPartFrom( pcSubBestPartCU, uiPartUnitIdx, uhNextDepth ); // 将最好的子块的数据存在当前的临时数据中
                }
            }/* {结束：枚举四叉树的子块} */
            
            m_pcRDGoOnSbacCoder->load(m_pppcRDSbacCoder[uhNextDepth][CI_NEXT_BEST]); // 使用现在（下一深度）的缓存初始RDO
            
            if( !bBoundary ) // 如果当前块不在边界，进行熵编码
            {
                m_pcEntropyCoder->resetBits(); // 重置码率
                m_pcEntropyCoder->encodeSplitFlag( rpcTempCU, 0, uiDepth, true ); // 对分割标志进行编码
                rpcTempCU->getTotalBits() += m_pcEntropyCoder->getNumberOfWrittenBits(); // split bits
                rpcTempCU->getTotalBins() += ((TEncBinCABAC *)((TEncSbac*)m_pcEntropyCoder->m_pcEntropyCoderIf)->getEncBinIf())->getBinsCoded(); // 计算熵编码码率
            }
            rpcTempCU->getTotalCost()  = m_pcRdCost->calcRdCost( rpcTempCU->getTotalBits(), rpcTempCU->getTotalDistortion() ); // 计算总的RD Cost
            
            if( uiDepth == pps.getMaxCuDQPDepth() && pps.getUseDQP()) // 如果使用DeltaQP且当前深度到达DeltaQP最大深度
            {
                Bool hasResidual = false; // 是否有残差的标识
                for( UInt uiBlkIdx = 0; uiBlkIdx < rpcTempCU->getTotalNumPart(); uiBlkIdx ++) // 枚举所有划分到最小的CU块
                {
                    if( (     rpcTempCU->getCbf(uiBlkIdx, COMPONENT_Y)
                         || (rpcTempCU->getCbf(uiBlkIdx, COMPONENT_Cb) && (numberValidComponents > COMPONENT_Cb))
                         || (rpcTempCU->getCbf(uiBlkIdx, COMPONENT_Cr) && (numberValidComponents > COMPONENT_Cr)) ) )
                    { // Cbf != 0 代表有残差
                        hasResidual = true; // 标识有残差为true
                        break;
                    }
                }
                
                if ( hasResidual ) // 如果有残差，进行熵编码
                {
                    m_pcEntropyCoder->resetBits();
                    m_pcEntropyCoder->encodeQP( rpcTempCU, 0, false );
                    rpcTempCU->getTotalBits() += m_pcEntropyCoder->getNumberOfWrittenBits(); // dQP bits
                    rpcTempCU->getTotalBins() += ((TEncBinCABAC *)((TEncSbac*)m_pcEntropyCoder->m_pcEntropyCoderIf)->getEncBinIf())->getBinsCoded();
                    rpcTempCU->getTotalCost()  = m_pcRdCost->calcRdCost( rpcTempCU->getTotalBits(), rpcTempCU->getTotalDistortion() ); // 之前都写过了
                    
                    Bool foundNonZeroCbf = false; // 找到非零cbf标识
                    rpcTempCU->setQPSubCUs( rpcTempCU->getRefQP( 0 ), 0, uiDepth, foundNonZeroCbf ); // 设置子块QP
                    assert( foundNonZeroCbf );
                }
                else // 所有最小CU都没有残差
                {
                    rpcTempCU->setQPSubParts( rpcTempCU->getRefQP( 0 ), 0, uiDepth ); // 将子块QP设置为默认值
                }
            } /* {结束：处理DeltaQP情况} */
            
            m_pcRDGoOnSbacCoder->store(m_pppcRDSbacCoder[uiDepth][CI_TEMP_BEST]); // 存储当前深度缓存的临时最优RD Cost
            
            
            if (rpcBestCU->getTotalCost() != MAX_DOUBLE) // 正在测试的配置没有超过最大字节数
            {
                const Bool isEndOfSlice        =    pcSlice->getSliceMode()==FIXED_NUMBER_OF_BYTES
                && ((pcSlice->getSliceBits()+rpcBestCU->getTotalBits())>pcSlice->getSliceArgument()<<3)
                && rpcBestCU->getCtuRsAddr() != pcPic->getPicSym()->getCtuTsToRsAddrMap(pcSlice->getSliceCurStartCtuTsAddr())
                && rpcBestCU->getCtuRsAddr() != pcPic->getPicSym()->getCtuTsToRsAddrMap(pcSlice->getSliceSegmentCurStartCtuTsAddr()); // 是否是Slice最末的标识
                const Bool isEndOfSliceSegment =    pcSlice->getSliceSegmentMode()==FIXED_NUMBER_OF_BYTES
                && ((pcSlice->getSliceSegmentBits()+rpcBestCU->getTotalBits()) > pcSlice->getSliceSegmentArgument()<<3)
                && rpcBestCU->getCtuRsAddr() != pcPic->getPicSym()->getCtuTsToRsAddrMap(pcSlice->getSliceSegmentCurStartCtuTsAddr()); // 是否是SS最末的标识
                
                if(isEndOfSlice || isEndOfSliceSegment) //由于切片段是切片的子集，因此不需要检查切片段的切片条件
                {
                    rpcBestCU->getTotalCost() = MAX_DOUBLE; // 如果是最末端，将RD Cost设置为最大字节数
                }
            }
            
            
            xCheckBestMode( rpcBestCU, rpcTempCU, uiDepth DEBUG_STRING_PASS_INTO(sDebug) DEBUG_STRING_PASS_INTO(sTempDebug) DEBUG_STRING_PASS_INTO(false) ); // 对RD Cost进行比较，检查最好的方式
            
        } /* {结束：枚举iQP} */
    } /* {结束：可以继续划分} */
    
    
    /* {子层和递归结束返回父层的每个块都要进行以下的部分} */
    
    rpcBestCU->copyToPic(uiDepth); // 复制最好方式的数据用于下一个块的预测
    xCopyYuv2Pic( rpcBestCU->getPic(), rpcBestCU->getCtuRsAddr(), rpcBestCU->getZorderIdxInCtu(), uiDepth, uiDepth ); // 复制预测图像和重建图像的YUV数据
    
    
}
/****************      Comment By HazelNut      ******************/
